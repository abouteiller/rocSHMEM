#include <nvshmem.h>

typedef int rocshmem_ctx_t;

#define rocshmem_global_exit(code) nvshmem_global_exit(code)
#define rocshmem_malloc(size) nvshmem_malloc(size)
#define rocshmem_free(ptr) nvshmem_free(ptr)

#define rocshmem_quiet() nvshmem_quiet()
#define rocshmem_fence() nvshmem_fence()

#define rocshmem_char_p(dest, source, pe) nvshmem_char_p(dest, source, pe)
#define rocshmem_putmem(dest, source, size, pe) nvshmem_putmem(dest, source, size, pe)
#define rocshmem_putmem_nbi(dest, source, size, pe) nvshmem_putmem_nbi(dest, source, size, pe)
#define rocshmem_putmem_wg(dest, source, size, pe) nvshmem_putmem_block(dest, source, size, pe)
#define rocshmem_putmem_nbi_wg(dest, source, size, pe) nvshmem_putmem_nbi_block(dest, source, size, pe)
#define rocshmem_putmem_wave(dest, source, size, pe) nvshmem_putmem_warp(dest, source, size, pe)
#define rocshmem_putmem_nbi_wave(dest, source, size, pe) nvshmem_putmem_nbi_warp(dest, source, size, pe)

#define rocshmem_char_g(source, pe) nvshmem_char_g(source, pe)
#define rocshmem_getmem(dest, source, size, pe) nvshmem_getmem(dest, source, size, pe)
#define rocshmem_getmem_nbi(dest, source, size, pe) nvshmem_getmem_nbi(dest, source, size, pe)
#define rocshmem_getmem_wg(dest, source, size, pe) nvshmem_getmem_block(dest, source, size, pe)
#define rocshmem_getmem_nbi_wg(dest, source, size, pe) nvshmem_getmem_nbi_block(dest, source, size, pe)
#define rocshmem_getmem_wave(dest, source, size, pe) nvshmem_getmem_warp(dest, source, size, pe)
#define rocshmem_getmem_nbi_wave(dest, source, size, pe) nvshmem_getmem_nbi_warp(dest, source, size, pe)

#define rocshmem_wg_init() do {} while(0)
#define rocshmem_wg_finalize() do {} while(0)
#define rocshmem_wg_ctx_create(ctx_type, ctx) do {} while(0)
#define rocshmem_wg_ctx_destroy(ctx) do {} while(0)

#define rocshmem_ctx_quiet(ctx) nvshmem_quiet()
#define rocshmem_ctx_fence(ctx) nvshmem_fence()

#define rocshmem_ctx_p(ctx, dest, source, size, pe) nvshmem_p(dest, source, size, pe)
#define rocshmem_ctx_putmem(ctx, dest, source, size, pe) nvshmem_putmem(dest, source, size, pe)
#define rocshmem_ctx_putmem_nbi(ctx, dest, source, size, pe) nvshmem_putmem_nbi(dest, source, size, pe)
#define rocshmem_ctx_putmem_wg(ctx, dest, source, size, pe) nvshmemx_putmem_block(dest, source, size, pe)
#define rocshmem_ctx_putmem_nbi_wg(ctx, dest, source, size, pe) nvshmemx_putmem_nbi_block(dest, source, size, pe)
#define rocshmem_ctx_putmem_wave(ctx, dest, source, size, pe) nvshmemx_putmem_warp(dest, source, size, pe)
#define rocshmem_ctx_putmem_nbi_wave(ctx, dest, source, size, pe) nvshmemx_putmem_nbi_warp(dest, source, size, pe)

#define rocshmem_ctx_g(ctx, dest, source, size, pe) nvshmem_g(dest, source, size, pe)
#define rocshmem_ctx_getmem(ctx, dest, source, size, pe) nvshmem_getmem(dest, source, size, pe)
#define rocshmem_ctx_getmem_nbi(ctx, dest, source, size, pe) nvshmem_getmem_nbi(dest, source, size, pe)
#define rocshmem_ctx_getmem_wg(ctx, dest, source, size, pe) nvshmemx_getmem_block(dest, source, size, pe)
#define rocshmem_ctx_getmem_nbi_wg(ctx, dest, source, size, pe) nvshmemx_getmem_nbi_block(dest, source, size, pe)
#define rocshmem_ctx_getmem_wave(ctx, dest, source, size, pe) nvshmemx_getmem_warp(dest, source, size, pe)
#define rocshmem_ctx_getmem_nbi_wave(ctx, dest, source, size, pe) nvshmemx_getmem_nbi_warp(dest, source, size, pe)

#define rocshmem_ctx_long_atomic_fetch_add(ctx, r_buf, val, pe) nvshmem_long_atomic_fetch_add(r_buf, val, pe)
#define rocshmem_ctx_long_atomic_fetch_inc(ctx, r_buf, pe) nvshmem_long_atomic_fetch_inc(r_buf, pe)
#define rocshmem_ctx_long_atomic_add(ctx, r_buf, val, pe) nvshmem_long_atomic_add(r_buf, val, pe)
#define rocshmem_ctx_long_atomic_inc(ctx, r_buf, pe) nvshmem_long_atomic_inc(r_buf, pe)
#define rocshmem_ctx_long_atomic_compare_swap(ctx, r_buf, cond, val, pe) nvshmem_long_atomic_compare_swap(r_buf, cond, val, pe)
