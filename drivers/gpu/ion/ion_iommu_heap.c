/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/err.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/iommu.h>
#include <linux/pfn.h>
#include "ion_priv.h"

#include <asm/mach/map.h>
#include <asm/page.h>
#include <mach/iommu_domains.h>

struct ion_iommu_heap {
	struct ion_heap heap;
};

struct ion_iommu_priv_data {
	struct page **pages;
	int nrpages;
	unsigned long size;
};

static int ion_iommu_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	int ret, i;
	struct ion_iommu_priv_data *data = NULL;

	if (msm_use_iommu()) {
		data = kmalloc(sizeof(*data), GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		data->size = PFN_ALIGN(size);
		data->nrpages = data->size >> PAGE_SHIFT;
		data->pages = kzalloc(sizeof(struct page *)*data->nrpages,
				GFP_KERNEL);
		if (!data->pages) {
			ret = -ENOMEM;
			goto err1;
		}

		for (i = 0; i < data->nrpages; i++) {
			data->pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
			if (!data->pages[i])
				goto err2;
		}


		buffer->priv_virt = data;
		return 0;

	} else {
		return -ENOMEM;
	}


err2:
	for (i = 0; i < data->nrpages; i++) {
		if (data->pages[i])
			__free_page(data->pages[i]);
	}
	kfree(data->pages);
err1:
	kfree(data);
	return ret;
}

static void ion_iommu_heap_free(struct ion_buffer *buffer)
{
	struct ion_iommu_priv_data *data = buffer->priv_virt;
	int i;

	if (!data)
		return;

	for (i = 0; i < data->nrpages; i++)
		__free_page(data->pages[i]);

	kfree(data->pages);
	kfree(data);
}

void *ion_iommu_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buffer,
				   unsigned long flags)
{
	struct ion_iommu_priv_data *data = buffer->priv_virt;
	pgprot_t page_prot = PAGE_KERNEL;

	if (!data)
		return NULL;

	if (!ION_IS_CACHED(flags))
		page_prot = pgprot_noncached(page_prot);

	buffer->vaddr = vmap(data->pages, data->nrpages, VM_IOREMAP, page_prot);

	return buffer->vaddr;
}

void ion_iommu_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	if (!buffer->vaddr)
		return;

	vunmap(buffer->vaddr);
	buffer->vaddr = NULL;
}

int ion_iommu_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			       struct vm_area_struct *vma, unsigned long flags)
{
	struct ion_iommu_priv_data *data = buffer->priv_virt;
	int i;

	if (!data)
		return -EINVAL;

	if (!ION_IS_CACHED(flags))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	for (i = 0; i < data->nrpages; i++)
		if (vm_insert_page(vma, vma->vm_start + i * PAGE_SIZE,
			data->pages[i]))
			/*
			 * This will fail the mmap which will
			 * clean up the vma space properly.
			 */
			return -EINVAL;

	return 0;
}

int ion_iommu_heap_map_iommu(struct ion_buffer *buffer,
					struct ion_iommu_map *data,
					unsigned int domain_num,
					unsigned int partition_num,
					unsigned long align,
					unsigned long iova_length,
					unsigned long flags)
{
	unsigned long temp_iova;
	struct iommu_domain *domain;
	struct ion_iommu_priv_data *buffer_data = buffer->priv_virt;
	int i, j, ret = 0;
	unsigned long extra;

	BUG_ON(!msm_use_iommu());

	data->mapped_size = iova_length;
	extra = iova_length - buffer->size;

	data->iova_addr = msm_allocate_iova_address(domain_num, partition_num,
						data->mapped_size, align);

	if (!data->iova_addr) {
		ret = -ENOMEM;
		goto out;
	}

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		ret = -ENOMEM;
		goto out1;
	}

	temp_iova = data->iova_addr;
	for (i = buffer->size, j = 0; i > 0; j++, i -= SZ_4K,
						temp_iova += SZ_4K) {
		ret = iommu_map(domain, temp_iova,
				page_to_phys(buffer_data->pages[j]),
				get_order(SZ_4K),
				ION_IS_CACHED(flags) ? 1 : 0);

		if (ret) {
			pr_err("%s: could not map %lx to %x in domain %p\n",
				__func__, temp_iova,
				page_to_phys(buffer_data->pages[j]),
				domain);
			goto out2;
		}
	}


	if (extra &&
		msm_iommu_map_extra
			(domain, temp_iova, extra, flags) < 0)
		goto out2;

	return 0;


out2:
	for ( ; i < buffer->size; i += SZ_4K, temp_iova -= SZ_4K)
		iommu_unmap(domain, temp_iova, get_order(SZ_4K));

out1:
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				buffer->size);

out:

	return ret;
}

void ion_iommu_heap_unmap_iommu(struct ion_iommu_map *data)
{
	int i;
	unsigned long temp_iova;
	unsigned int domain_num;
	unsigned int partition_num;
	struct iommu_domain *domain;

	BUG_ON(!msm_use_iommu());

	domain_num = iommu_map_domain(data);
	partition_num = iommu_map_partition(data);

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		WARN(1, "Could not get domain %d. Corruption?\n", domain_num);
		return;
	}

	temp_iova = data->iova_addr;
	for (i = data->mapped_size; i > 0; i -= SZ_4K, temp_iova += SZ_4K)
		iommu_unmap(domain, temp_iova, get_order(SZ_4K));

	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				data->mapped_size);

	return;
}

static int ion_iommu_cache_ops(struct ion_heap *heap, struct ion_buffer *buffer,
			void *vaddr, unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	unsigned long vstart, pstart;
	void (*op)(unsigned long, unsigned long, unsigned long);
	unsigned int i;
	struct ion_iommu_priv_data *data = buffer->priv_virt;

	if (!data)
		return -ENOMEM;

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
		op = clean_caches;
		break;
	case ION_IOC_INV_CACHES:
		op = invalidate_caches;
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		op = clean_and_invalidate_caches;
		break;
	default:
		return -EINVAL;
	}

	vstart = (unsigned long) vaddr;
	for (i = 0; i < data->nrpages; ++i, vstart += PAGE_SIZE) {
		pstart = page_to_phys(data->pages[i]);
		op(vstart, PAGE_SIZE, pstart);
	}

	return 0;
}

static struct scatterlist *ion_iommu_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{
	struct scatterlist *sglist = NULL;
	if (buffer->priv_virt) {
		struct ion_iommu_priv_data *data = buffer->priv_virt;
		unsigned int i;

		if (!data->nrpages)
			return NULL;

		sglist = vmalloc(sizeof(*sglist) * data->nrpages);
		if (!sglist)
			return ERR_PTR(-ENOMEM);

		sg_init_table(sglist, data->nrpages);
		for (i = 0; i < data->nrpages; ++i)
			sg_set_page(&sglist[i], data->pages[i], PAGE_SIZE, 0);
	}
	return sglist;
}

static void ion_iommu_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	if (buffer->sglist)
		vfree(buffer->sglist);
}

static struct ion_heap_ops iommu_heap_ops = {
	.allocate = ion_iommu_heap_allocate,
	.free = ion_iommu_heap_free,
	.map_user = ion_iommu_heap_map_user,
	.map_kernel = ion_iommu_heap_map_kernel,
	.unmap_kernel = ion_iommu_heap_unmap_kernel,
	.map_iommu = ion_iommu_heap_map_iommu,
	.unmap_iommu = ion_iommu_heap_unmap_iommu,
	.cache_op = ion_iommu_cache_ops,
	.map_dma = ion_iommu_heap_map_dma,
	.unmap_dma = ion_iommu_heap_unmap_dma,
};

struct ion_heap *ion_iommu_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_iommu_heap *iommu_heap;

	iommu_heap = kzalloc(sizeof(struct ion_iommu_heap), GFP_KERNEL);
	if (!iommu_heap)
		return ERR_PTR(-ENOMEM);

	iommu_heap->heap.ops = &iommu_heap_ops;
	iommu_heap->heap.type = ION_HEAP_TYPE_IOMMU;

	return &iommu_heap->heap;
}

void ion_iommu_heap_destroy(struct ion_heap *heap)
{
	struct ion_iommu_heap *iommu_heap =
	     container_of(heap, struct  ion_iommu_heap, heap);

	kfree(iommu_heap);
	iommu_heap = NULL;
}
