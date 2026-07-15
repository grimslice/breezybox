/*
 * usb_midi.c - first-steps USB-MIDI host bring-up for the Tanmatsu USB-A port.
 *
 * The `midi` console command enables the 5V boost on the host port (same call
 * the native launcher makes), installs the USB host stack, registers a client,
 * then stays in the FOREGROUND streaming diagnostics until q/Ctrl-C. When a
 * device appears it prints the descriptors, claims the first MIDI streaming
 * interface (class 0x01 subclass 0x03), and dumps incoming USB-MIDI event
 * packets (4 bytes: cable/CIN + 3 MIDI bytes) decoded as note on/off etc.
 *
 * All USB work happens on background tasks, but those tasks' printf may not
 * reach the console the command was typed on (stdio is per-vterm here), so
 * they report through a message queue that the foreground loop drains.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_console.h"
#include "usb/usb_host.h"
#include "bsp/power.h"

static const char *TAG = "usb_midi";

static bool                     s_started;
static usb_host_client_handle_t s_client;
static usb_device_handle_t      s_dev;
static uint8_t                  s_midi_intf;
static bool                     s_claimed;
static usb_transfer_t          *s_xfer_in;

/* ---- Message queue: USB tasks -> foreground console loop ---- */

#define MSG_LEN 96
static QueueHandle_t s_msgq;

static void midi_logf(const char *fmt, ...)
{
    char line[MSG_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (!s_msgq || xQueueSend(s_msgq, line, 0) != pdTRUE)
        ESP_LOGI(TAG, "%s", line);   /* queue full/missing: at least hit the log */
}

/* ---- MIDI packet decode (USB-MIDI 1.0: 32-bit event packets) ---- */

static void print_midi_packet(const uint8_t *p)
{
    uint8_t cin = p[0] & 0x0f;
    uint8_t ch = p[1] & 0x0f;

    if (cin == 0x9 && p[3] != 0)
        midi_logf("MIDI ch%-2d NOTE ON  note=%-3d vel=%d", ch + 1, p[2], p[3]);
    else if (cin == 0x8 || (cin == 0x9 && p[3] == 0))
        midi_logf("MIDI ch%-2d NOTE OFF note=%d", ch + 1, p[2]);
    else if (cin == 0xB)
        midi_logf("MIDI ch%-2d CC       cc=%-3d val=%d", ch + 1, p[2], p[3]);
    else if (cin != 0)   /* CIN 0 = reserved/misc, usually padding */
        midi_logf("MIDI raw cin=%X: %02X %02X %02X", cin, p[1], p[2], p[3]);
}

static void in_xfer_cb(usb_transfer_t *xfer)
{
    if (xfer->status == USB_TRANSFER_STATUS_COMPLETED) {
        for (int i = 0; i + 4 <= xfer->actual_num_bytes; i += 4)
            print_midi_packet(xfer->data_buffer + i);
    } else if (xfer->status != USB_TRANSFER_STATUS_CANCELED &&
               xfer->status != USB_TRANSFER_STATUS_NO_DEVICE) {
        midi_logf("IN transfer status %d", xfer->status);
    }
    if (s_dev && xfer->status != USB_TRANSFER_STATUS_NO_DEVICE)
        usb_host_transfer_submit(xfer);   /* keep listening */
}

/* ---- Enumeration: walk config descriptor, claim MIDI streaming intf ---- */

static void probe_device(void)
{
    const usb_device_desc_t *dd;
    if (usb_host_get_device_descriptor(s_dev, &dd) != ESP_OK) {
        midi_logf("failed to read device descriptor");
        return;
    }
    midi_logf("USB device: VID=%04X PID=%04X (class %02X)",
              dd->idVendor, dd->idProduct, dd->bDeviceClass);

    const usb_config_desc_t *cfg;
    if (usb_host_get_active_config_descriptor(s_dev, &cfg) != ESP_OK) {
        midi_logf("failed to read config descriptor");
        return;
    }

    const uint8_t *p   = (const uint8_t *)cfg;
    const uint8_t *end = p + cfg->wTotalLength;
    int in_midi = -1;   /* interface number once found */

    for (p += p[0]; p + 2 <= end && p[0] >= 2; p += p[0]) {
        if (p[1] == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            const usb_intf_desc_t *id = (const usb_intf_desc_t *)p;
            midi_logf("  intf %d alt %d: class %02X/%02X proto %02X, %d eps",
                      id->bInterfaceNumber, id->bAlternateSetting,
                      id->bInterfaceClass, id->bInterfaceSubClass,
                      id->bInterfaceProtocol, id->bNumEndpoints);
            /* Audio class (0x01), MIDI Streaming subclass (0x03) */
            in_midi = (id->bInterfaceClass == 0x01 && id->bInterfaceSubClass == 0x03)
                      ? id->bInterfaceNumber : -1;
            if (in_midi >= 0 && !s_claimed) {
                if (usb_host_interface_claim(s_client, s_dev,
                                             id->bInterfaceNumber,
                                             id->bAlternateSetting) == ESP_OK) {
                    s_midi_intf = id->bInterfaceNumber;
                    s_claimed = true;
                    midi_logf("  -> claimed MIDI streaming interface %d", s_midi_intf);
                }
            }
        } else if (p[1] == USB_B_DESCRIPTOR_TYPE_ENDPOINT && in_midi >= 0 && s_claimed) {
            const usb_ep_desc_t *ep = (const usb_ep_desc_t *)p;
            bool is_in   = ep->bEndpointAddress & 0x80;
            bool is_bulk = (ep->bmAttributes & 0x03) == USB_TRANSFER_TYPE_BULK;
            midi_logf("    ep %02X %s mps=%d", ep->bEndpointAddress,
                      is_bulk ? "bulk" : "other", ep->wMaxPacketSize);
            if (is_in && is_bulk && !s_xfer_in) {
                if (usb_host_transfer_alloc(ep->wMaxPacketSize, 0, &s_xfer_in) == ESP_OK) {
                    s_xfer_in->device_handle    = s_dev;
                    s_xfer_in->bEndpointAddress = ep->bEndpointAddress;
                    s_xfer_in->num_bytes        = ep->wMaxPacketSize;
                    s_xfer_in->callback         = in_xfer_cb;
                    esp_err_t err = usb_host_transfer_submit(s_xfer_in);
                    midi_logf("    -> IN listen: %s", esp_err_to_name(err));
                }
            }
        }
    }
    if (!s_claimed)
        midi_logf("  no MIDI streaming interface found on this device");
}

static void drop_device(void)
{
    if (s_xfer_in) {
        usb_host_endpoint_halt(s_dev, s_xfer_in->bEndpointAddress);
        usb_host_endpoint_flush(s_dev, s_xfer_in->bEndpointAddress);
        usb_host_transfer_free(s_xfer_in);
        s_xfer_in = NULL;
    }
    if (s_claimed) {
        usb_host_interface_release(s_client, s_dev, s_midi_intf);
        s_claimed = false;
    }
    if (s_dev) {
        usb_host_device_close(s_client, s_dev);
        s_dev = NULL;
    }
}

/* ---- Host library + client plumbing (same shape as the launcher) ---- */

static void client_event_cb(const usb_host_client_event_msg_t *msg, void *arg)
{
    switch (msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            midi_logf("USB: new device, addr %d", msg->new_dev.address);
            if (!s_dev) {
                if (usb_host_device_open(s_client, msg->new_dev.address, &s_dev) == ESP_OK)
                    probe_device();
                else
                    midi_logf("USB: device open failed");
            }
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            midi_logf("USB: device gone");
            if (s_dev && msg->dev_gone.dev_hdl == s_dev) drop_device();
            break;
        default:
            midi_logf("USB: client event %d", msg->event);
            break;
    }
}

static void usb_lib_task(void *arg)
{
    const usb_host_config_t cfg = { .intr_flags = ESP_INTR_FLAG_LOWMED };
    esp_err_t err = usb_host_install(&cfg);
    midi_logf("usb_host_install: %s", esp_err_to_name(err));
    xTaskNotifyGive(arg);
    if (err != ESP_OK) vTaskDelete(NULL);
    while (true) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags) midi_logf("usb lib event flags: %08lx", (unsigned long)flags);
    }
}

static void midi_client_task(void *arg)
{
    const usb_host_client_config_t cfg = {
        .is_synchronous    = false,
        .max_num_event_msg = 8,
        .async = { .client_event_callback = client_event_cb },
    };
    esp_err_t err = usb_host_client_register(&cfg, &s_client);
    midi_logf("usb_host_client_register: %s", esp_err_to_name(err));
    xTaskNotifyGive(arg);
    if (err != ESP_OK) vTaskDelete(NULL);
    while (true)
        usb_host_client_handle_events(s_client, portMAX_DELAY);
}

/* ---- Foreground command ---- */

static int cmd_midi(int argc, char **argv)
{
    if (!s_started) {
        s_msgq = xQueueCreate(64, MSG_LEN);

        esp_err_t err = bsp_power_set_usb_host_boost_enabled(true);
        printf("USB host 5V boost set: %s\n", esp_err_to_name(err));
        bool on = false;
        err = bsp_power_get_usb_host_boost_enabled(&on);
        printf("USB host 5V boost readback: %s (%s)\n",
               on ? "ON" : "OFF", esp_err_to_name(err));

        /* Verbose stack logs -> UART/JTAG log console, to see root-port
         * activity (connect/reset/enum) even if no client event fires. */
        esp_log_level_set("USB HOST", ESP_LOG_DEBUG);
        esp_log_level_set("HCD DWC", ESP_LOG_DEBUG);
        esp_log_level_set("HUB", ESP_LOG_DEBUG);
        esp_log_level_set("ENUM", ESP_LOG_DEBUG);
        esp_log_level_set("USBH", ESP_LOG_DEBUG);

        xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096,
                                xTaskGetCurrentTaskHandle(), 2, NULL, 0);
        ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(1000));
        xTaskCreatePinnedToCore(midi_client_task, "usb_midi", 4096,
                                xTaskGetCurrentTaskHandle(), 2, NULL, 0);
        ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(1000));
        s_started = true;
    }

    printf("USB MIDI monitor: plug/replug the device now. q or Ctrl-C to exit.\n");

    int old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);

    char line[MSG_LEN];
    int idle_ticks = 0;
    bool running = true;
    while (running) {
        bool got = false;
        while (s_msgq && xQueueReceive(s_msgq, line, 0) == pdTRUE) {
            printf("%s\n", line);
            got = true;
        }
        char ch;
        while (read(STDIN_FILENO, &ch, 1) == 1)
            if (ch == 'q' || ch == 3) running = false;

        if (got) idle_ticks = 0;
        else if (++idle_ticks >= 500) {   /* ~5 s heartbeat */
            idle_ticks = 0;
            usb_host_lib_info_t info;
            if (usb_host_lib_info(&info) == ESP_OK)
                printf("... waiting (devices: %d, clients: %d)\n",
                       info.num_devices, info.num_clients);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    fcntl(STDIN_FILENO, F_SETFL, old_flags);
    printf("midi: monitor stopped (USB host stack stays up)\n");
    return 0;
}

void usb_midi_register_command(void)
{
    const esp_console_cmd_t cmd = {
        .command = "midi",
        .help    = "USB host port power + USB-MIDI monitor (foreground)",
        .func    = &cmd_midi,
    };
    esp_console_cmd_register(&cmd);
}
