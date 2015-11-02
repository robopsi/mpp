/*
 * Copyright 2010 Rockchip Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mpp_buffer.h"
#include "vpu_mem_legacy.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "vpu.h"
#include <string.h>

#ifdef ANDROID
#include <linux/ion.h>
#endif

static RK_S32 commit_memory_handle(vpu_display_mem_pool *p, RK_S32 mem_hdl, RK_S32 size)
{
    MppBufferInfo info;

    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl *)p;
    memset(&info, 0, sizeof(MppBufferInfo));
    info.type = MPP_BUFFER_TYPE_ION;
    info.fd = mem_hdl;
    info.size = size;
    p_mempool->size = size;
    mpp_buffer_commit(p_mempool->group, &info);
    return info.fd;
}

static void* get_free_memory_vpumem(vpu_display_mem_pool *p)
{
    MPP_RET ret = MPP_OK;
    MppBuffer buffer = NULL;
    VPUMemLinear_t *dmabuf = mpp_calloc(VPUMemLinear_t, 1);
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl *)p;
    if (dmabuf == NULL) {
        return NULL;
    }
    ret = mpp_buffer_get(p_mempool->group, &buffer, p_mempool->size);
    if (MPP_OK != ret) {
        mpp_free(dmabuf);
        return NULL;
    }
    dmabuf->phy_addr = (RK_U32)mpp_buffer_get_fd(buffer);
    dmabuf->vir_addr = (RK_U32*)mpp_buffer_get_ptr(buffer);
    dmabuf->size = p_mempool->size;
    dmabuf->offset = (RK_U32*)buffer;
    return dmabuf;

}

static RK_S32 inc_used_memory_handle_ref(vpu_display_mem_pool *p, void * hdl)
{
    (void)p;
    VPUMemLinear_t *dmabuf = (VPUMemLinear_t *)hdl;
    MppBuffer buffer = (MppBuffer)dmabuf->offset;
    if (buffer != NULL) {
        mpp_buffer_inc_ref(buffer);
    }
    return MPP_OK;

}

static RK_S32 put_used_memory_handle(vpu_display_mem_pool *p, void *hdl)
{
    (void)p;
    VPUMemLinear_t *dmabuf = (VPUMemLinear_t *)hdl;
    MppBuffer buf = (MppBuffer)dmabuf->offset;
    if (buf != NULL) {
        mpp_buffer_put(buf);
    }
    return MPP_OK;
}

static RK_S32 get_free_memory_num(vpu_display_mem_pool *p)
{
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl *)p;
    if (p_mempool->group != NULL) {
        return mpp_buffer_group_unused(p_mempool->group);
    }
    return 0;
}

static RK_S32 reset_vpu_mem_pool(vpu_display_mem_pool *p)
{
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl *)p;
    mpp_buffer_group_clear(p_mempool->group);
    return 0;

}


vpu_display_mem_pool* open_vpu_memory_pool()
{
    mpp_err("open_vpu_memory_pool in\n");
    vpu_display_mem_pool_impl *p_mempool = mpp_calloc(vpu_display_mem_pool_impl, 1);

    if (NULL == p_mempool) {
        return NULL;
    }
    mpp_buffer_group_get_external(&p_mempool->group, MPP_BUFFER_TYPE_ION);
    if (NULL == p_mempool->group) {
        return NULL;
    }
    p_mempool->commit_hdl     = commit_memory_handle;
    p_mempool->get_free       = get_free_memory_vpumem;
    p_mempool->put_used       = put_used_memory_handle;
    p_mempool->inc_used       = inc_used_memory_handle_ref;
    p_mempool->reset          = reset_vpu_mem_pool;
    p_mempool->get_unused_num = get_free_memory_num;
    p_mempool->version        = 1;
    p_mempool->buff_size      = -1;
    return (vpu_display_mem_pool*)p_mempool;
}

void close_vpu_memory_pool(vpu_display_mem_pool *p)
{
    mpp_err("close_vpu_memory_pool in xxxxxxxxxxxxxxx");
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl *)p;
    mpp_buffer_group_put(p_mempool->group);
    mpp_free(p_mempool);
    return;
}

int create_vpu_memory_pool_allocator(vpu_display_mem_pool **ipool, int num, int size)
{
    (void)num;
    vpu_display_mem_pool_impl *p_mempool = mpp_calloc(vpu_display_mem_pool_impl, 1);
    if (NULL == p_mempool) {
        return -1;
    }
    mpp_buffer_group_get_internal(&p_mempool->group, MPP_BUFFER_TYPE_ION);
    if (NULL == p_mempool->group) {
        return -1;
    }
    p_mempool->commit_hdl     = commit_memory_handle;
    p_mempool->get_free       = get_free_memory_vpumem;
    p_mempool->put_used       = put_used_memory_handle;
    p_mempool->inc_used       = inc_used_memory_handle_ref;
    p_mempool->reset          = reset_vpu_mem_pool;
    p_mempool->get_unused_num = get_free_memory_num;
    p_mempool->version        = 0;
    p_mempool->buff_size      = size;
    *ipool = (vpu_display_mem_pool*)p_mempool;
    return 0;
}

void release_vpu_memory_pool_allocator(vpu_display_mem_pool *ipool)
{
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl *)ipool;
    if (p_mempool == NULL) {
        return;
    }
    if (NULL != p_mempool->group) {
        mpp_buffer_group_put(p_mempool->group);
    }
    mpp_free(p_mempool);
    return;
}

RK_S32 VPUMemJudgeIommu()
{
    int ret = 0;
#ifdef ANDROID
    if (VPUClientGetIOMMUStatus() > 0) {
        //mpp_err("media.used.iommu");
        ret = 1;
    }
#endif
    return ret;
}


RK_S32 VPUMallocLinear(VPUMemLinear_t *p, RK_U32 size)
{
    MppBuffer buffer = NULL;
    mpp_buffer_get(NULL, &buffer, size);
    p->phy_addr = (RK_U32)mpp_buffer_get_fd(buffer);
    p->vir_addr = (RK_U32*)mpp_buffer_get_ptr(buffer);
    p->size = size;
    p->offset = (RK_U32*)buffer;
    return 0;
}

RK_S32 VPUMallocLinearFromRender(VPUMemLinear_t *p, RK_U32 size, void *ctx)
{
    VPUMemLinear_t *dma_buf = NULL;
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl *)ctx;
    dma_buf = (VPUMemLinear_t *)p_mempool->get_free((vpu_display_mem_pool *)ctx);
    if (dma_buf != NULL) {
        if (dma_buf->size < size) {
            mpp_free(dma_buf);
            return -1;
        }
        memcpy(p, dma_buf, sizeof(VPUMemLinear_t));
        mpp_free(dma_buf);
    }
    return 0;
}


RK_S32 VPUFreeLinear(VPUMemLinear_t *p)
{
    if (p->offset != NULL) {
        put_used_memory_handle(NULL, p);
    }
    return 0;
}


RK_S32 VPUMemDuplicate(VPUMemLinear_t *dst, VPUMemLinear_t *src)
{
    MppBuffer buffer = (MppBuffer)src->offset;
    if (buffer != NULL) {
        mpp_buffer_inc_ref(buffer);
    }
    memcpy(dst, src, sizeof(VPUMemLinear_t));
    return 0;
}

RK_S32 VPUMemLink(VPUMemLinear_t *p)
{
    (void)p;
    return 0;
}

RK_S32 VPUMemFlush(VPUMemLinear_t *p)
{
    (void)p;
    return 0;
}

RK_S32 VPUMemClean(VPUMemLinear_t *p)
{
    (void)p;
    return 0;
}


RK_S32 VPUMemInvalidate(VPUMemLinear_t *p)
{
    (void)p;
    return 0;
}

RK_S32 VPUMemGetFD(VPUMemLinear_t *p)
{
    RK_S32 fd = 0;
    MppBuffer buffer = (MppBuffer)p->offset;
    fd = mpp_buffer_get_fd(buffer);
    return fd;
}

RK_S32 vpu_mem_judge_used_heaps_type()
{
    // TODO, use property_get
#if 0//def ANDROID
    if (!VPUClientGetIOMMUStatus() > 0) {
        return ION_HEAP(ION_CMA_HEAP_ID);
    } else {
        ALOGV("USE ION_SYSTEM_HEAP");
        return ION_HEAP(ION_VMALLOC_HEAP_ID);
    }
#endif

    return 0;
}
