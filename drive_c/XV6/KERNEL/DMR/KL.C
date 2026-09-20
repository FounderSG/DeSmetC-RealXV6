/*
 *   KL/DL-11 driver
 */
#include <os.h>

#define NKL11   1
#define NDL11   0

struct  tty kl11[NKL11+NDL11];
int KLADDR[4] = { -1, -1, -1, -1 };

int klopen(dev, flag)
int dev;
int flag;
{
    register struct tty *tp;

    if(minor(dev) >= NKL11+NDL11) {
        u.u_error = ENXIO;
        return 0;
    }
    tp = &kl11[minor(dev)];
    if (u.u_procp->p_ttyp == 0) {
        u.u_procp->p_ttyp = tp;
        tp->t_dev = dev;
    }
    /*
     * set up minor 0 to address KLADDR
     * set up minor 1 thru NKL11-1 to address from KLBASE
     * set up minor NKL11 on to address from DLBASE
     */
    tp->t_addr = KLADDR;
    if ((tp->t_state&ISOPEN) == 0) {
        tp->t_state = ISOPEN|CARR_ON;
        tp->t_flags = XTABS|ECHO|CRMOD;
        tp->t_erase = CERASE;
        tp->t_kill = CKILL;
    }

    (void)flag;
    return 0;
}

int klclose(dev, flag)
int dev;
int flag;
{
    register struct tty *tp;

    tp = &kl11[minor(dev)];
    wflushtty(tp);
    /* tp->t_state = 0; */

    (void)flag;
    return 0;
}

int klread(dev)
int dev;
{
    ttread(&kl11[minor(dev)]);
    return 0;
}

int klwrite(dev)
int dev;
{
#ifdef KL_BACKEND_UART
    ttwrite(&kl11[minor(dev)]);
#else
    int c;
    while ((c=cpass())>=0) {
        if(c=='\t') { cnputc(' '); c = ' '; }
        if(c=='\n') cnputc('\r');
        cnputc(c);
    }
#endif
    return 0;
}

int klxint(dev)
int dev;
{
    struct tty *tp;
    int c;

    tp = &kl11[minor(dev)];
    ttstart(tp);
    if (tp->t_outq.c_cc == 0 || tp->t_outq.c_cc == TTLOWAT)
        wakeup(&tp->t_outq);

    return 0;
}

int klrint(dev)
int dev;
{
    register int c;
    register struct tty *tp;

#ifdef KL_BACKEND_UART
    c = uart_getc();
#else
#ifdef KL_BACKEND_KBD
    c = kbd_getc();
#else
    no KL_BACKEND defined;    /* 3.03 has neither #elif nor #error */
#endif
#endif
    tp = &kl11[minor(dev)];
    if (tp->t_state&ISOPEN) {
        if(c > 0) {
            ttyinput(c, tp);
        }
    }

    return 0;
}

int klsgtty(dev, v)
int dev;
int *v;
{
    register struct tty *tp;

    tp = &kl11[minor(dev)];
    ttystty(tp, v);
    return 0;
}

void klrxintr()
{
    klrint(0);
}

void kltxintr()
{
    KLADDR[2] |= DONE;
    klxint(0);
}
