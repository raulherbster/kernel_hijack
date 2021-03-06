#include <linux/module.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/unistd.h>

#include "sldlib.h"

#ifdef CONFIG_IA32_EMULATION
#include "unistd_32.h"
#endif


#ifdef __i386__
struct idt_descriptor {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char type_flags;
    unsigned short offset_high;
} __attribute__ ((packed));
#elif defined(CONFIG_IA32_EMULATION)
struct idt_descriptor {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char zero1;
    unsigned char type_flags;
    unsigned short offset_middle;
    unsigned int offset_high;
    unsigned int zero2;
} __attribute__ ((packed));
#endif

struct idtr {
    unsigned short limit;
    void *base;
} __attribute__ ((packed));


void **sys_call_table;
#ifdef CONFIG_IA32_EMULATION
void **ia32_sys_call_table;
#endif


asmlinkage long (*real_seal)(uid_t uid);

asmlinkage long (*real_is_sealed)(uid_t uid);

asmlinkage long (*real_sld_create_key)(uid_t uid);

asmlinkage long (*real_sld_open)(uid_t uid);

asmlinkage long (*real_sld_ssl_connect)(uid_t uid);

asmlinkage long (*real_sld_ssl_read)(uid_t uid);

asmlinkage long (*real_sld_ssl_write)(uid_t uid);

asmlinkage long (*real_sld_ssl_disconnect)(uid_t uid);

asmlinkage long (*real_sld_post)(uid_t uid);

asmlinkage long (*real_sld_put)(uid_t uid);

asmlinkage long (*real_sld_get)(uid_t uid);

asmlinkage long (*real_sld_delete)(uid_t uid);

asmlinkage long hooked_seal(uid_t uid) {
    return 157;
}

asmlinkage long hooked_is_sealed(uid_t uid) {
    return 157;
}

asmlinkage long hooked_sld_create_key(uid_t uid) {
    return 157;
}

asmlinkage long hooked_sld_open(uid_t uid) {
    return 157;
}

asmlinkage long hooked_sld_ssl_connect(uid_t uid) {
    return 157;
}

asmlinkage long hooked_sld_ssl_read(uid_t uid) {
    return 157;
}

asmlinkage long hooked_sld_ssl_write(uid_t uid) {
    return 157;
}

asmlinkage long hooked_sld_ssl_disconnect(uid_t uid) {
    return 157;
}

asmlinkage long hooked_sld_post(uid_t uid) {
    return 157;
}

asmlinkage long hooked_sld_put(uid_t uid) {
    return 157;
}

asmlinkage long hooked_sld_get(uid_t uid) {
    return 157;
}

asmlinkage long hooked_sld_delete(uid_t uid) {
    return 157;
}

#if defined(__i386__) || defined(CONFIG_IA32_EMULATION)
#ifdef __i386__
void *get_sys_call_table(void) {
#elif defined(__x86_64__)
void *get_ia32_sys_call_table(void) {
#endif
    struct idtr idtr;
    struct idt_descriptor idtd;
    void *system_call;
    unsigned char *ptr;
    int i;

    asm volatile("sidt %0" : "=m"(idtr));

    memcpy(&idtd, idtr.base + 0x80*sizeof(idtd), sizeof(idtd));

#ifdef __i386__
    system_call = (void*)((idtd.offset_high<<16) | idtd.offset_low);
#elif defined(__x86_64__)
    system_call = (void*)(((long)idtd.offset_high<<32) |
                        (idtd.offset_middle<<16) | idtd.offset_low);
#endif

    for (ptr=system_call, i=0; i<500; i++) {
#ifdef __i386__
        if (ptr[0] == 0xff && ptr[1] == 0x14 && ptr[2] == 0x85)
            return *((void**)(ptr+3));
#elif defined(__x86_64__)
        if (ptr[0] == 0xff && ptr[1] == 0x14 && ptr[2] == 0xc5)
            return (void*) (0xffffffff00000000 | *((unsigned int*)(ptr+3)));
#endif
        ptr++;
    }

    return NULL;
}
#endif


#ifdef __x86_64__
#define IA32_LSTAR  0xc0000082

void *get_sys_call_table(void) {
    void *system_call;
    unsigned char *ptr;
    int i, low, high;

    asm volatile("rdmsr" : "=a" (low), "=d" (high) : "c" (IA32_LSTAR));

    system_call = (void*)(((long)high<<32) | low);

    for (ptr=system_call, i=0; i<500; i++) {
        if (ptr[0] == 0xff && ptr[1] == 0x14 && ptr[2] == 0xc5)
            return (void*)(0xffffffff00000000 | *((unsigned int*)(ptr+3)));
        ptr++;
    }   

    return NULL;
}
#endif


void *get_writable_sct(void *sct_addr) {
    struct page *p[2];
    void *sct;
    unsigned long addr = (unsigned long)sct_addr & PAGE_MASK;

    if (sct_addr == NULL)
        return NULL;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22) && defined(__x86_64__)
    p[0] = pfn_to_page(__pa_symbol(addr) >> PAGE_SHIFT);
    p[1] = pfn_to_page(__pa_symbol(addr + PAGE_SIZE) >> PAGE_SHIFT);
#else
    p[0] = virt_to_page(addr);
    p[1] = virt_to_page(addr + PAGE_SIZE);
#endif
    sct = vmap(p, 2, VM_MAP, PAGE_KERNEL);
    if (sct == NULL)
        return NULL;
    return sct + offset_in_page(sct_addr);
}

static int __init hook_init(void) {
    sys_call_table = get_writable_sct(get_sys_call_table());
    if (sys_call_table == NULL)
        return -1;

#ifdef CONFIG_IA32_EMULATION
    ia32_sys_call_table = get_writable_sct(get_ia32_sys_call_table());
    if (ia32_sys_call_table == NULL) {
        vunmap((void*)((unsigned long)sys_call_table & PAGE_MASK));
        return -1;
    }
#endif

    /* hook seal */
#ifdef __NR_seal32
    real_seal = sys_call_table[__NR_seal32];
    real_is_sealed = sys_call_table[__NR_is_sealed32];
    real_sld_create_key = sys_call_table[__NR_sld_create_key32];
    real_sld_open = sys_call_table[__NR_sld_open32];
    real_sld_ssl_connect = sys_call_table[__NR_sld_ssl_connect32];
    real_sld_ssl_read = sys_call_table[__NR_sld_ssl_read32];
    real_sld_ssl_write = sys_call_table[__NR_sld_ssl_write32];
    real_sld_ssl_disconnect = sys_call_table[__NR_sld_ssl_disconnect32];
    real_sld_post = sys_call_table[__NR_sld_put32];
    real_sld_put = sys_call_table[__NR_sld_put32];
    real_sld_get = sys_call_table[__NR_sld_get32];
    real_sld_delete = sys_call_table[__NR_sld_delete32];

    sys_call_table[__NR_seal32] = hooked_seal;
    sys_call_table[__NR_is_sealed32] = hooked_is_sealed;
    sys_call_table[__NR_sld_create_key32] = hooked_create_key;
    sys_call_table[__NR_sld_open32] = hooked_sld_open;
    sys_call_table[__NR_sld_ssl_connect32] = hooked_sld_ssl_connect;
    sys_call_table[__NR_sld_ssl_read32] = hooked_sld_ssl_read;
    sys_call_table[__NR_sld_ssl_write32] = hooked_sld_ssl_write;
    sys_call_table[__NR_sld_ssl_disconnect32] = hooked_sld_ssl_disconnect;
    sys_call_table[__NR_sld_post32] = hooked_post;
    sys_call_table[__NR_sld_put32] = hooked_sld_put;
    sys_call_table[__NR_sld_get32] = hooked_sld_get;
    sys_call_table[__NR_sld_delete32] = hooked_sld_delete;
#else
    real_seal = sys_call_table[__NR_seal];
    real_is_sealed = sys_call_table[__NR_is_sealed];
    real_sld_create_key = sys_call_table[__NR_sld_create_key];
    real_sld_open = sys_call_table[__NR_sld_open];
    real_sld_ssl_connect = sys_call_table[__NR_sld_ssl_connect];
    real_sld_ssl_read = sys_call_table[__NR_sld_ssl_read];
    real_sld_ssl_write = sys_call_table[__NR_sld_ssl_write];
    real_sld_ssl_disconnect = sys_call_table[__NR_sld_ssl_disconnect];
    real_sld_post = sys_call_table[__NR_sld_put];
    real_sld_put = sys_call_table[__NR_sld_put];
    real_sld_get = sys_call_table[__NR_sld_get];
    real_sld_delete = sys_call_table[__NR_sld_delete];

    sys_call_table[__NR_seal] = hooked_seal;
    sys_call_table[__NR_is_sealed] = hooked_is_sealed;
    sys_call_table[__NR_sld_create_key] = hooked_sld_create_key;
    sys_call_table[__NR_sld_open] = hooked_sld_open;
    sys_call_table[__NR_sld_ssl_connect] = hooked_sld_ssl_connect;
    sys_call_table[__NR_sld_ssl_read] = hooked_sld_ssl_read;
    sys_call_table[__NR_sld_ssl_write] = hooked_sld_ssl_write;
    sys_call_table[__NR_sld_ssl_disconnect] = hooked_sld_ssl_disconnect;
    sys_call_table[__NR_sld_post] = hooked_sld_post;
    sys_call_table[__NR_sld_put] = hooked_sld_put;
    sys_call_table[__NR_sld_get] = hooked_sld_get;
    sys_call_table[__NR_sld_delete] = hooked_sld_delete;
#endif
    /***************/

    return 0;
}

static void __exit hook_exit(void) {
    /* unhook seal */
#ifdef __NR_seal32
    sys_call_table[__NR_seal32] = real_seal;
    sys_call_table[__NR_is_sealed32] = real_is_sealed;
    sys_call_table[__NR_sld_create_key32] = real_sld_create_key;
    sys_call_table[__NR_sld_open32] = real_sld_open;
    sys_call_table[__NR_sld_ssl_connect32] = real_sld_ssl_connect;
    sys_call_table[__NR_sld_ssl_read32] = real_sld_ssl_read;
    sys_call_table[__NR_sld_ssl_write32] = real_sld_ssl_write;
    sys_call_table[__NR_sld_ssl_disconnect32] = real_sld_ssl_disconnect;
    sys_call_table[__NR_sld_post32] = real_sld_post;
    sys_call_table[__NR_sld_put32] = real_sld_put;
    sys_call_table[__NR_sld_get32] = real_sld_get;
    sys_call_table[__NR_sld_delete32] = real_sld_delete;

#else
    sys_call_table[__NR_seal] = real_seal;
    sys_call_table[__NR_is_sealed] = real_is_sealed;
    sys_call_table[__NR_sld_create_key] = real_sld_create_key;
    sys_call_table[__NR_sld_open] = real_sld_open;
    sys_call_table[__NR_sld_ssl_connect] = real_sld_ssl_connect;
    sys_call_table[__NR_sld_ssl_read] = real_sld_ssl_read;
    sys_call_table[__NR_sld_ssl_write] = real_sld_ssl_write;
    sys_call_table[__NR_sld_ssl_disconnect] = real_sld_ssl_disconnect;
    sys_call_table[__NR_sld_post] = real_sld_post;
    sys_call_table[__NR_sld_put] = real_sld_put;
    sys_call_table[__NR_sld_get] = real_sld_get;
    sys_call_table[__NR_sld_delete] = real_sld_delete;
#endif
    /*****************/

    // unmap memory
    vunmap((void*)((unsigned long)sys_call_table & PAGE_MASK));
#ifdef CONFIG_IA32_EMULATION
    vunmap((void*)((unsigned long)ia32_sys_call_table & PAGE_MASK));
#endif
}

module_init(hook_init);
module_exit(hook_exit);
MODULE_LICENSE("GPL");
