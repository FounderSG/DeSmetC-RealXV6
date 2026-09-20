/* os.h is needed for uint and the peekw/pokew segment accessors: the
   transfer buffer lives outside the kernel's data segment. */
#include <os.h>

/* Important bits in the status register of an ATA controller.
   See ATA/ATAPI-4 spec, section 7.15.6 */
#define IDE_BSY 0x80
#define IDE_DRDY 0x40
#define IDE_DF 0x20
#define IDE_ERR 0x01

/* ATA protocol commands. */
#define IDE_CMD_READ 0x20
#define IDE_CMD_WRITE 0x30

#ifndef outportb
void outport();
void outportb();
unsigned inport();
unsigned char inportb();
#endif

void rkintr();

static int io_sector;
static int io_count;
static int io_cmd;
/*
 * The transfer buffer as a (segment, offset) pair: it lives outside the
 * kernel's data segment (buffer cache, a swapped page, a raw user buffer)
 * and DeSmet C has no far pointer to hold it.  Advancing io_bufoff by 512
 * per sector is what "io_buf += 256" did on the int far * it replaces.
 */
static uint io_bufseg;
static uint io_bufoff;

/* Wait for IDE disk to become ready. */
int idewait(checkerr)
int checkerr;
{
    int r;

    while (((r = inportb(0x1f7)) & (IDE_BSY | IDE_DRDY)) != IDE_DRDY)
        ;
    if (checkerr && (r & (IDE_DF | IDE_ERR)) != 0)
        return -1;
    return 0;
}

/*
 * Start the request for IDE disk.  "wr" replaces the old convention of
 * passing a null data pointer for a read: the buffer is now two words, so
 * there is no single value left to mean "none".
 */
void idestart(sector, wr, dseg, doff)
int sector;
int wr;
uint dseg;
uint doff;
{
    int i;

    idewait(0);
    outportb(0x3f6, 0); /* generate interrupt */
    outportb(0x1f2, 1); /* number of sectors */
    outportb(0x1f3, sector & 0xff);
    outportb(0x1f4, (sector >> 8) & 0xff);
    outportb(0x1f5, 0);
    outportb(0x1f6, 0xe0);
    if (wr)
    {
        outportb(0x1f7, IDE_CMD_WRITE);
        for (i = 0; i < 256; i++)
            outport(0x1f0, peekw(dseg, doff + i*2));
    }
    else
    {
        outportb(0x1f7, IDE_CMD_READ);
    }
}

void ideio(sector, count, bufseg, bufoff, cmd)
int sector;
int count;
uint bufseg;
uint bufoff;
int cmd;
{
    io_sector = sector;
    io_count = count;
    io_bufseg = bufseg;
    io_bufoff = bufoff;
    io_cmd = cmd;

    idestart(io_sector, cmd == 0, io_bufseg, io_bufoff);
}

void ideintr()
{
    int i;

    io_sector++;
    io_count--;

    if (io_cmd != 0)
    {
        for (i = 0; i < 256; i++)
            pokew(io_bufseg, io_bufoff + i*2, inport(0x1f0));
    }
    io_bufoff += 512;

    if (io_count <= 0)
    {
        rkintr();
    }else{
        idestart(io_sector, io_cmd == 0, io_bufseg, io_bufoff);
    }
}
