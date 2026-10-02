#include "../include/nova/types.h"
#include "../include/nova/io.h"
#include "../include/nova/ata.h"

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_SECCOUNT 0x1F2
#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_COMMAND 0x1F7
#define ATA_ALTSTATUS 0x3F6
#define ATA_CONTROL 0x3F6

#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

static int present;

static u8 ata_status(void) {
    return inb(ATA_STATUS);
}

static void ata_delay(void) {
    inb(ATA_ALTSTATUS);
    inb(ATA_ALTSTATUS);
    inb(ATA_ALTSTATUS);
    inb(ATA_ALTSTATUS);
}

static int ata_wait(u8 wanted) {
    for (u32 i = 0; i < 1000000u; ++i) {
        u8 s = ata_status();
        if (s & ATA_SR_ERR)
            return 0;
        if (!(s & ATA_SR_BSY) && (s & wanted))
            return 1;
    }
    return 0;
}

static int ata_wait_ready(void) {
    for (u32 i = 0; i < 1000000u; ++i) {
        u8 s = ata_status();
        if (s & ATA_SR_ERR)
            return 0;
        if (!(s & ATA_SR_BSY) && !(s & ATA_SR_DRQ))
            return 1;
    }
    return 0;
}

void ata_init(void) {
    present = 0;
    outb(ATA_DRIVE, 0xA0);
    ata_delay();
    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA0, 0);
    outb(ATA_LBA1, 0);
    outb(ATA_LBA2, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay();

    u8 s = ata_status();
    if (!s || s == 0xFF)
        return;
    if (!ata_wait(ATA_SR_DRQ))
        return;

    for (u32 i = 0; i < 256u; ++i)
        (void)inb(ATA_DATA), (void)inb(ATA_DATA);

    present = 1;
}

int ata_present(void) {
    return present;
}

static int ata_rw28(u32 lba, u8 count, void *buffer, int write) {
    if (!present || !buffer || !count || lba > 0x0FFFFFFFu)
        return 0;

    u8 *p = (u8 *)buffer;

    for (u32 sector = 0; sector < count; ++sector) {
        u32 cur = lba + sector;
        outb(ATA_DRIVE, 0xE0u | (u8)((cur >> 24) & 0x0Fu));
        outb(ATA_SECCOUNT, 1);
        outb(ATA_LBA0, (u8)cur);
        outb(ATA_LBA1, (u8)(cur >> 8));
        outb(ATA_LBA2, (u8)(cur >> 16));
        outb(ATA_COMMAND, write ? ATA_CMD_WRITE : ATA_CMD_READ);

        if (!ata_wait(ATA_SR_DRQ))
            return 0;

        u16 *words = (u16 *)(p + sector * 512u);
        for (u32 i = 0; i < 256u; ++i) {
            if (write)
                __asm__ volatile ("outw %0,%1" :: "a"(words[i]), "Nd"((u16)ATA_DATA));
            else
                __asm__ volatile ("inw %1,%0" : "=a"(words[i]) : "Nd"((u16)ATA_DATA));
        }
        if (write)
            ata_wait_ready();
    }

    return 1;
}

int ata_read28(u32 lba, u8 count, void *buffer) {
    return ata_rw28(lba, count, buffer, 0);
}

int ata_write28(u32 lba, u8 count, const void *buffer) {
    return ata_rw28(lba, count, (void *)buffer, 1);
}
