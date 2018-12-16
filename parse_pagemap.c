#define _LARGEFILE64_SOURCE 
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <stddef.h>

#define MAX_FILEPATH (100)

static const char anonymous_name[] = "[anonymous_mapping]";
static int print_all_pages = 0;

//keep file descriptos global for efficency
static int pagemap_fd;
static FILE *map_fp = NULL;
static int kpageflgs_fd;
static int kpagecnt_fd;

struct map_info
{
	ptrdiff_t start;
	ptrdiff_t end;
	char read;
	char write;
	char execute;
	char shared;
	unsigned long long fileoff;
	unsigned int dev_major;
	unsigned int dev_minor;
	unsigned long inode;
	char *name;
};

struct page_info
{
	unsigned int in_ram;
	unsigned int in_swap;
	unsigned int shared;
	unsigned int exclusive;
	unsigned int softdirty;
	unsigned long pfn;
	unsigned long flags;
	unsigned long pgcnt;
};


int page_info_cmp(struct page_info *prev, struct page_info *curr)
{
	//pfn is either 0 or sequential
	return ((curr->in_ram    == prev->in_ram) &&
		(curr->in_swap   == prev->in_swap) &&
		(curr->shared    == prev->shared) &&
		(curr->exclusive == prev->exclusive) &&
		(curr->softdirty == prev->softdirty) &&
		(curr->flags     == prev->flags) &&
		(curr->pgcnt     == prev->pgcnt) &&
	       ((curr->pfn       == prev->pfn &&
		 curr->pfn	 == 0) ||
	        (curr->pfn       == (prev->pfn + 1))));
}

void print_section(ptrdiff_t start, size_t size, struct page_info *pginfo, struct map_info  *map)
{
	if(pginfo->in_ram) {
		printf("0x%016lx 0x%016lx 0x%016lx %c%c%c%c %d 0x%016lx %lu %s\n",
				start, size, pginfo->pfn,
				map->read, map->write, map->execute, map->shared,
				pginfo->exclusive, pginfo->flags, pginfo->pgcnt,
				map->name);
	} else {
		printf("0x%016lx 0x%016lx %s %c%c%c%c %d 0x%016lx %lu %s\n",
				start, size, "[notinram]",
				map->read, map->write, map->execute, map->shared,
				pginfo->exclusive, pginfo->flags, pginfo->pgcnt,
				map->name);
	}

} 

void traverse_pageinfo(struct map_info *map)
{
	long pagesz = sysconf(_SC_PAGESIZE);
	unsigned long tmpaddr;
	struct page_info prev_pginfo = {0,0,0,0,0,0,0,0};
	ptrdiff_t sec_start = map->start;

	for (tmpaddr = map->start; tmpaddr <= map->end; tmpaddr += pagesz) {
		struct page_info pginfo = {0,0,0,0,0,0,0,0};
		uint64_t entry;

		if (lseek(pagemap_fd, (tmpaddr / pagesz) * 8, SEEK_SET) == -1) {
			perror("unable to seek in pagemap");
		} else if(read(pagemap_fd, &entry, 8) != 8) {
			perror("unable to read in pagemap");
		} else {
			//63 present in ram
			pginfo.in_ram    = (entry >> 63) & 0x1;
			//62, present in swap
			pginfo.in_swap   = (entry >> 62) & 0x1;
			//61, shared
			pginfo.shared    = (entry >> 61) & 0x1;
			//56, exclusive
			pginfo.exclusive = (entry >> 56) & 0x1;
			//55 softdirty
			pginfo.softdirty = (entry >> 55) & 0x1;
			//54-0 is physical frame number or swap offset
			pginfo.pfn       = entry & (((uint64_t)1 << 55) - 1);
		
			/* each page has a 64bit word description */
			if( lseek(kpageflgs_fd, pginfo.pfn * 8, SEEK_SET) == -1) {
				perror("unable to seek in kpageflags");
			} else if (read(kpageflgs_fd, &pginfo.flags, 8) != 8) {
				perror("unable to read kpageflags");
			}
		
			if (lseek(kpagecnt_fd, pginfo.pfn * 8, SEEK_SET) == -1) {
				perror("unable to seek in kpagecnt");
			} else if (read(kpagecnt_fd, &pginfo.pgcnt, 8) != 8) {
				perror("unable to read kpagecnt");
			} 
		}
		if(print_all_pages) {
			print_section(tmpaddr, pagesz, &pginfo, map);
		/* print if not the same attribues as previous. Never print first time unless
		   first time is also the last. Always print the last iteration */
		}else if((!page_info_cmp(&prev_pginfo, &pginfo) && tmpaddr != map->start) ||
			tmpaddr == map->end) {
			//so this one differs, print the previous and
			//save this
			print_section(sec_start, tmpaddr - sec_start, &prev_pginfo, map);

			sec_start = tmpaddr;
		}
		prev_pginfo = pginfo;
	}
	//print last one if have tried to summarize (and not just printed)
	//if not, this has already been printed
	if(!print_all_pages)  {
		print_section(sec_start, tmpaddr - sec_start, &prev_pginfo, map);
	}
}

int main(int argc, char *argv[])
{
	int pid;
	char pagemap_filename[MAX_FILEPATH];
	char map_filename[MAX_FILEPATH];
	char kpageflgs_filename[] = "/proc/kpageflags";
	char kpagecnt_filename[] = "/proc/kpagecount";
	char *mapline = NULL;
	size_t line_length;

	if(argc > 2 && !strcmp(argv[1], "--combine")) {
		print_all_pages = 0;
		pid = atoi(argv[2]);
	} else if (argc == 2) {
		print_all_pages = 1;
		pid = atoi(argv[1]);
	} else {
		printf("usage: %s [--combine] <pid>\n", argv[0]);
		exit(1);
	}


	snprintf(pagemap_filename, MAX_FILEPATH, "/proc/%d/pagemap",pid);
	snprintf(map_filename, MAX_FILEPATH, "/proc/%d/maps",pid);

	pagemap_fd = open(pagemap_filename, O_RDONLY);
	if(pagemap_fd == -1) {
		fprintf(stderr, "unable to open %s\n\terror: %d, %s\n",
			pagemap_filename, errno, strerror(errno));
		goto error;
	}
	/* Use fopen for pagemap, this is a textfile and we will read
	 * the entire file in one sweep. So buffering is probably good
	 * Other files will be seeked a lot in. Also, getline is used
	 * not sure if there is a corresponding function for file
	 * descriptors? */
	map_fp = fopen(map_filename, "r");
	if(map_fp == NULL) {
		fprintf(stderr, "unable to open %s\n\terror: %d, %s\n",
			map_filename, errno, strerror(errno));
		goto error;
	}
	kpageflgs_fd = open(kpageflgs_filename, O_RDONLY);
	if(kpageflgs_fd == -1) {
		fprintf(stderr, "unable to open %s\n\terror: %d, %s\n",
			kpageflgs_filename, errno, strerror(errno));
		goto error;
	}
	kpagecnt_fd = open(kpagecnt_filename, O_RDONLY);
	if(kpagecnt_fd == -1) {
		fprintf(stderr, "unable to open %s\n\terror: %d, %s\n",
			kpagecnt_filename, errno, strerror(errno));
		goto error;
	}

	printf("vaddr size pfn type exclusive flags mappings name\n");

	while(getline(&mapline, &line_length, map_fp) != -1) {
		struct map_info map;
		int items;

		map.name = NULL;

		/*
		 * we should found at least 10 items, 11 if its not an anonymous
		 * mmap (80 char breaked due to not break strings)
		 */
		items = sscanf(mapline, "%lx-%lx %c%c%c%c %llx %x:%x %lu%*[ \n\t]%m[^\n]",
				&map.start, &map.end, &map.read,
				&map.write, &map.execute,
				&map.shared, &map.fileoff,
				&map.dev_major, &map.dev_minor,
				&map.inode, &map.name);
		if (items < 10) {
			printf(">>>>WARNING: row (%.20s...) not formatted as expected, skipping\n",
				mapline);
		} else {

			if(map.name == NULL) {
				map.name = (char*)anonymous_name;
			}

			traverse_pageinfo(&map);

			if(map.name != anonymous_name) {
				free(map.name);
			}
		}
		free(mapline);
		mapline = NULL;
	}
error:
	if(pagemap_fd)
		close(pagemap_fd);
	if(map_fp)
		fclose(map_fp);
	if(kpageflgs_fd)
		close(kpageflgs_fd);
	if(kpagecnt_fd)
		close(kpagecnt_fd);

	return 0;
}
