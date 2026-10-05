/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Negative control: the former transport, compiled only in ignored host output.
 * Its FILE API pairing matches the reviewed ESP32-S31 Picolibc disassembly:
 * ftrylockfile acquires the global libc recursive lock and returns zero;
 * funlockfile releases FILE->_lock, not that global lock.
 */
static size_t send_packet(const mosaico_diagnostics_packet_t *packet, size_t offset)
{
    if (offset >= packet->len || ftrylockfile(stdout) != 0) {
        return 0;
    }
    bool separate_stderr = stderr != stdout;
    if (separate_stderr && ftrylockfile(stderr) != 0) {
        funlockfile(stdout);
        return 0;
    }
    size_t remaining = packet->len - offset;
    size_t queued = 0;
    if (tud_cdc_n_write_available(TINYUSB_CDC_ACM_0) >= remaining) {
        queued = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0,
                                           packet->bytes + offset, remaining);
    }
    (void)tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
    if (separate_stderr) {
        funlockfile(stderr);
    }
    funlockfile(stdout);
    return queued;
}
