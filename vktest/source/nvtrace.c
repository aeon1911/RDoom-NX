/* Diagnostic: logs the nvhost-as-gpu calls Mesa makes (link with --wrap). */
#include <switch.h>
#include <stdio.h>

void vk_logmsg(const char *fmt, ...);

Result __real_nvioctlNvhostAsGpu_MapBufferEx(u32 fd, u32 flags, u32 kind, u32 h, u32 page_size, u64 buffer_offset, u64 mapping_size, u64 input_offset, u64 *offset);
Result __wrap_nvioctlNvhostAsGpu_MapBufferEx(u32 fd, u32 flags, u32 kind, u32 h, u32 page_size, u64 buffer_offset, u64 mapping_size, u64 input_offset, u64 *offset)
{
	Result rc = __real_nvioctlNvhostAsGpu_MapBufferEx(fd, flags, kind, h, page_size, buffer_offset, mapping_size, input_offset, offset);
	vk_logmsg("nv MapBufferEx flags=0x%x kind=0x%x handle=%u page_size=0x%x buf_off=0x%llx size=0x%llx in_off=0x%llx -> rc=0x%x out=0x%llx",
		flags, kind, h, page_size, (unsigned long long)buffer_offset, (unsigned long long)mapping_size, (unsigned long long)input_offset, rc,
		offset ? (unsigned long long)*offset : 0ULL);
	return rc;
}
Result __real_nvioctlNvhostAsGpu_GetVARegions(u32 fd, nvioctl_va_region regions[2]);
Result __wrap_nvioctlNvhostAsGpu_GetVARegions(u32 fd, nvioctl_va_region regions[2])
{
	Result rc = __real_nvioctlNvhostAsGpu_GetVARegions(fd, regions);
	for (int i = 0; i < 2; i++)
		vk_logmsg("nv VA region %d: offset=0x%llx page_size=0x%x pages=0x%llx (end 0x%llx) rc=0x%x", i,
			(unsigned long long)regions[i].offset, regions[i].page_size, (unsigned long long)regions[i].pages,
			(unsigned long long)(regions[i].offset + regions[i].pages * regions[i].page_size), rc);
	return rc;
}
