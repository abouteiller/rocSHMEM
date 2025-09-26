/******************************************************************************
 * Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *****************************************************************************/

//TODO copied from bnxt as-is

#include "gda/backend_gda.hpp"
#include "util.hpp"
#include <unistd.h> // getpagesize()

namespace rocshmem {

void GDABackend::ionic_initialize_gpu_qp(QueuePair* gpu_qp, int conn_num) {
  struct ionic_re_dv_obj dv_obj;
  struct ionic_re_dv_cq dv_cq;
  struct ionic_re_dv_qp dv_qp;
  struct ibv_qp *ib_qp;
  int err;

  ib_qp = qps[conn_num];

  /* Export CQ */
  memset(&dv_obj, 0, sizeof(struct ionic_re_dv_obj));
  dv_obj.cq.in  = cqs[conn_num];
  dv_obj.cq.out = &dv_cq;

  err = ionicdv_ftable_.init_obj(&dv_obj, ionic_RE_DV_OBJ_CQ);
  CHECK_ZERO(err, "ionic_re_dv_init_obj(CQ)");

  memset(&gpu_qp->cq, 0, sizeof(ionic_device_cq));
  gpu_qp->cq.buf   = ionic_cqs[conn_num].buf;
  gpu_qp->cq.depth = ionic_cqs[conn_num].depth;
  gpu_qp->cq.id    = dv_cq.cqn;
  gpu_qp->cq.phase = ionic_RE_QUEUE_START_PHASE;

  /* Export QP */
  memset(&dv_obj, 0, sizeof(struct ionic_re_dv_obj));
  dv_obj.qp.in  = ib_qp;
  dv_obj.qp.out = &dv_qp;

  err = ionicdv_ftable_.init_obj(&dv_obj, ionic_RE_DV_OBJ_QP);
  CHECK_ZERO(err, "ionic_re_dv_init_obj(QP)");

  memset(&gpu_qp->sq, 0, sizeof(ionic_device_sq));
  gpu_qp->sq.buf        = ionic_qps[conn_num].sq_buf;
  gpu_qp->sq.depth      = ionic_qps[conn_num].mem_info.sq_slots;

  if ((gpu_qp->sq.depth % ionic_RE_STATIC_WQE_BB) != 0) {
    fprintf(stderr,
            "[WARNING] SQ depth not divisible by ionic_RE_STATIC_WQE_BB. "
            "There may be runtime errors.\n");
  }

  gpu_qp->sq.id          = ib_qp->qp_num;
  gpu_qp->sq.msntbl      = ionic_qps[conn_num].msntbl;
  gpu_qp->sq.msn_tbl_sz  = ionic_qps[conn_num].msn_tbl_sz;
  gpu_qp->sq.psn_sz_log2 = std::log2(ionic_qps[conn_num].mem_info.sq_psn_sz);
  gpu_qp->sq.mtu         = ibv_mtu_to_int(portinfo.active_mtu);

  /* Export DB */
  err = ionicdv_ftable_.get_default_db_region(context, &db_region_attr);
  CHECK_ZERO(err, "ionic_re_dv_init_obj(QP)");

  CHECK_HIP(hipHostRegister(db_region_attr.dbr, getpagesize(), hipHostRegisterDefault));
  CHECK_HIP(hipHostGetDevicePointer((void**) &gpu_qp->dbr, db_region_attr.dbr, 0));

  /* Export Memory Keys */
  gpu_qp->lkey = heap_mr->lkey;
  gpu_qp->rkey = heap_rkey[conn_num % num_pes];

  /* Export Inline Threshold */
  gpu_qp->inline_threshold = inline_threshold;
}

void GDABackend::ionic_create_cqs(int cqe) {
  struct ionic_re_dv_cq_attr cq_attr;
  struct ionic_re_dv_cq_init_attr cq_init_attr;
  struct ionic_re_dv_umem_reg_attr umem_attr;

  for (int i = 0; i < qps.size(); i++) {
    /* Allocate CQ mem */
    memset(&cq_attr, 0, sizeof(struct ionic_re_dv_cq_attr));
    ionic_cqs[i].handle = ionicdv_ftable_.cq_mem_alloc(context, cqe, &cq_attr);
    CHECK_NNULL(ionic_cqs[i].handle, "ionic_re_dv_cq_mem_alloc");

    /* Allocate CQ UMEM */
    ionic_cqs[i].length = cq_attr.ncqe * cq_attr.cqe_size;
    ionic_cqs[i].depth  = cq_attr.ncqe;
    CHECK_HIP(hipExtMallocWithFlags(&ionic_cqs[i].buf, ionic_cqs[i].length, hipDeviceMallocUncached));

    /* Register CQ UMEM */
    memset(&umem_attr, 0, sizeof(struct ionic_re_dv_umem_reg_attr));
    umem_attr.addr         = ionic_cqs[i].buf;
    umem_attr.size         = ionic_cqs[i].length;
    umem_attr.access_flags = IBV_ACCESS_LOCAL_WRITE;

    ionic_cqs[i].umem_handle = ionicdv_ftable_.umem_reg(context, &umem_attr);
    CHECK_NNULL(ionic_cqs[i].umem_handle, "ionic_re_dv_umem_reg(cq_buf)");

    /* Create CQ */
    memset(&cq_init_attr, 0, sizeof(struct ionic_re_dv_cq_init_attr));
    cq_init_attr.cq_handle   = (uint64_t) ionic_cqs[i].handle;
    cq_init_attr.umem_handle = ionic_cqs[i].umem_handle;
    cq_init_attr.ncqe        = cq_attr.ncqe;

    cqs[i] = ionicdv_ftable_.create_cq(context, &cq_init_attr);
    CHECK_NNULL(cqs[i], "ionic_re_dv_create_cq");
  }
}

void GDABackend::ionic_create_qps(int sq_length) {
  struct ibv_qp_init_attr ib_qp_attr;
  struct ionic_re_dv_umem_reg_attr umem_attr;
  void *sq_ptr;
  void *rq_ptr;
  void* sq_umem_handle;
  void* rq_umem_handle;
  uint64_t msntbl_len;
  uint64_t msntbl_offset;
  int err;

  for (int i = 0; i < qps.size(); i++) {
    /* IB QP Init Attr */
    memset(&ib_qp_attr, 0, sizeof(struct ibv_qp_init_attr));
    ib_qp_attr.send_cq             = cqs[i];
    ib_qp_attr.recv_cq             = cqs[i];
    ib_qp_attr.cap.max_send_wr     = sq_length;
    ib_qp_attr.cap.max_recv_wr     = 0;
    ib_qp_attr.cap.max_send_sge    = 1;
    ib_qp_attr.cap.max_recv_sge    = 0;
    ib_qp_attr.cap.max_inline_data = inline_threshold;
    ib_qp_attr.qp_type             = IBV_QPT_RC;
    ib_qp_attr.sq_sig_all          = 0;

    /* Alloc qp_mem_info */
    memset(&ionic_qps[i].mem_info, 0, sizeof(struct ionic_re_dv_qp_mem_info));
    err = ionicdv_ftable_.qp_mem_alloc(pd_orig, &ib_qp_attr, &ionic_qps[i].mem_info);
    CHECK_ZERO(err, "ionic_re_dv_qp_mem_alloc");

    /* Alloc SQ */
    CHECK_HIP(hipExtMallocWithFlags(&sq_ptr, ionic_qps[i].mem_info.sq_len, hipDeviceMallocUncached));
    ionic_qps[i].mem_info.sq_va = (uint64_t) sq_ptr;
    ionic_qps[i].sq_buf = sq_ptr;

    /* Obtain MSN Table Pointer */
    msntbl_len             = (ionic_qps[i].mem_info.sq_psn_sz * ionic_qps[i].mem_info.sq_npsn);
    msntbl_offset          = ionic_qps[i].mem_info.sq_len - msntbl_len;
    ionic_qps[i].msntbl     = (void*) ((char*) ionic_qps[i].sq_buf + msntbl_offset);
    ionic_qps[i].msn_tbl_sz = ionic_qps[i].mem_info.sq_npsn;

    /* Alloc RQ */
    CHECK_HIP(hipExtMallocWithFlags(&rq_ptr, ionic_qps[i].mem_info.rq_len, hipDeviceMallocUncached));
    ionic_qps[i].mem_info.rq_va = (uint64_t) rq_ptr;
    ionic_qps[i].rq_buf = rq_ptr;

    /* Register UMEM */
    memset(&umem_attr, 0, sizeof(struct ionic_re_dv_umem_reg_attr));
    umem_attr.addr         = (void*) ionic_qps[i].mem_info.sq_va;
    umem_attr.size         = ionic_qps[i].mem_info.sq_len;
    umem_attr.access_flags = IBV_ACCESS_LOCAL_WRITE;

    sq_umem_handle = ionicdv_ftable_.umem_reg(context, &umem_attr);
    CHECK_NNULL(sq_umem_handle, "ionic_re_dv_umem_reg(sq)");

    memset(&umem_attr, 0, sizeof(struct ionic_re_dv_umem_reg_attr));
    umem_attr.addr         = (void*) ionic_qps[i].mem_info.rq_va;
    umem_attr.size         = ionic_qps[i].mem_info.rq_len;
    umem_attr.access_flags = IBV_ACCESS_LOCAL_WRITE;

    rq_umem_handle = ionicdv_ftable_.umem_reg(context, &umem_attr);
    CHECK_NNULL(rq_umem_handle, "ionic_re_dv_umem_reg(rq)");

    /* IB DV QP Init Attr */
    memset(&ionic_qps[i].attr, 0, sizeof(struct ionic_re_dv_qp_init_attr));
    ionic_qps[i].attr.send_cq         = ib_qp_attr.send_cq;
    ionic_qps[i].attr.recv_cq         = ib_qp_attr.recv_cq;
    ionic_qps[i].attr.max_send_wr     = ib_qp_attr.cap.max_send_wr;
    ionic_qps[i].attr.max_recv_wr     = ib_qp_attr.cap.max_recv_wr;
    ionic_qps[i].attr.max_send_sge    = ib_qp_attr.cap.max_send_sge;
    ionic_qps[i].attr.max_recv_sge    = ib_qp_attr.cap.max_recv_sge;
    ionic_qps[i].attr.max_inline_data = ib_qp_attr.cap.max_inline_data;
    ionic_qps[i].attr.qp_type         = ib_qp_attr.qp_type;

    ionic_qps[i].attr.qp_handle = ionic_qps[i].mem_info.qp_handle;
    ionic_qps[i].attr.sq_umem_handle = sq_umem_handle;
    ionic_qps[i].attr.sq_len    = ionic_qps[i].mem_info.sq_len;
    ionic_qps[i].attr.sq_slots  = ionic_qps[i].mem_info.sq_slots;
    ionic_qps[i].attr.sq_wqe_sz = ionic_qps[i].mem_info.sq_wqe_sz;
    ionic_qps[i].attr.sq_psn_sz = ionic_qps[i].mem_info.sq_psn_sz;
    ionic_qps[i].attr.sq_npsn   = ionic_qps[i].mem_info.sq_npsn;

    ionic_qps[i].attr.rq_umem_handle = rq_umem_handle;
    ionic_qps[i].attr.rq_len    = ionic_qps[i].mem_info.rq_len;
    ionic_qps[i].attr.rq_slots  = ionic_qps[i].mem_info.rq_slots;
    ionic_qps[i].attr.rq_wqe_sz = ionic_qps[i].mem_info.rq_wqe_sz;
    ionic_qps[i].attr.comp_mask = ionic_qps[i].mem_info.comp_mask;

    /* Alloc QP */
    qps[i] = ionicdv_ftable_.create_qp(pd_orig, &ionic_qps[i].attr);
    CHECK_NNULL(qps[i], "ionic_re_dv_create_qp");
  }
}

int GDABackend::ionic_dv_dl_init() {
  ionicdv_handle_ = dlopen("libionic.so", RTLD_NOW);
  if (!ionicdv_handle_) {
    // Try hard-coded PATH
    ionicdv_handle_ = dlopen("/usr/local/lib/libionic.so", RTLD_NOW);
    if (!ionicdv_handle_) {
      DPRINTF("Could not open libionic.so. Returning\n");
      return ROCSHMEM_ERROR;
    }
  }

  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, init_obj);
  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, create_qp);
  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, destroy_qp);
  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, modify_qp);
  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, qp_mem_alloc);
  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, create_cq);
  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, destroy_cq);
  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, cq_mem_alloc);
  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, umem_reg);
  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, umem_dereg);
  DLSYM_HELPER(ionicdv_ftable_, ionic_re_dv_, ionicdv_handle_, get_default_db_region);

  return ROCSHMEM_SUCCESS;
}

}  // namespace rocshmem
