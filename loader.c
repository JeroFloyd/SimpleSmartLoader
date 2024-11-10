#define _GNU_SOURCE
#include "loader.h"
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>



Elf32_Ehdr *ehd;
Elf32_Phdr *phd;
int FileDesc;
int PF = 0;
int PageAllocs = 0;
size_t IntFrag = 0;
size_t PageSize;


void LoaderClean(){ //This cleans up resources of the loader
    ehd = NULL;
    phd = NULL;
    if(FileDesc >= 0){
        close(FileDesc);
    } 
}


char *ElfFileReader(const char *file_name, off_t *DescSize){
    FileDesc = open(file_name, O_RDONLY);
    if(FileDesc<0){
        perror("Error: Can't open ELF file!");
        exit(1);
    }

    *DescSize = lseek(FileDesc, 0, SEEK_END);
    lseek(FileDesc, 0, SEEK_SET);
    char *HeapMem = (char *)malloc(*DescSize);

    if(!HeapMem){
        perror("Error: Memory allocation has failed!");
        close(FileDesc);
        exit(1);
    }

    ssize_t file_read = read(FileDesc, HeapMem, *DescSize);

    if(file_read<0 || (size_t)file_read!=*DescSize){
        perror("Error: File read operation has failed!");
        free(HeapMem);
        close(FileDesc);
        exit(1);
    }
    return HeapMem;
}


void *PageMap(Elf32_Phdr *seg, char *heap, void *fa){ //This maps a 4kb page for a seg when needed
    void *AllignedAddress = (void *)((uintptr_t)fa&~(PageSize-1));
    size_t SegOffset = (uintptr_t)AllignedAddress-seg->p_vaddr;

    size_t pages = (seg->p_memsz+PageSize-1)/PageSize; //The total no. of pages required for this segment is calculated by this
    size_t CurrPage = SegOffset/PageSize;
    int LastPage = (CurrPage == pages-1);
    void *MappedMemory = mmap(AllignedAddress, PageSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    
    if(MappedMemory == MAP_FAILED){
        perror("Error: mmap failed!");
        exit(1);
    }
    if(SegOffset < seg->p_filesz){
        size_t bytes_to_copy = (seg->p_filesz - SegOffset) < PageSize ? (seg->p_filesz - SegOffset) : PageSize;
        memcpy(MappedMemory, heap + seg->p_offset + SegOffset, bytes_to_copy);
    }
    PageAllocs++;
    if(LastPage){
        size_t UnusedBytes = PageSize-(seg->p_memsz % PageSize);
        if(UnusedBytes != PageSize){  //This counts fragmentation only if there is unused space
            IntFrag += UnusedBytes;
            printf("Internal fragmentation calculated for last page: %zu KB\n", IntFrag/1024);
        }
    }
    return MappedMemory;
}


void EntryPointFin(unsigned int *entry){
    for(int i =0; i<ehd->e_phnum; i++){
        if(phd[i].p_type == PT_LOAD){
            *entry = ehd->e_entry;
        }
    }
}


void *EntryPointFinder(unsigned int *entry){ //This is tp locate the entry point address
    for(int i =0; i<ehd->e_phnum; i++){
        if(phd[i].p_type == PT_LOAD){
            *entry = ehd->e_entry;
            return (void *)(*entry);
        }
    }
    return NULL;
}


void HandlePageFault(int sig, siginfo_t *info, void *context) { //This is a signal handler for segation faults
    void *fa = info->si_addr;
    for(int i=0; i<ehd->e_phnum; i++){
        if(phd[i].p_type == PT_LOAD && (uintptr_t)fa >= phd[i].p_vaddr && (uintptr_t)fa < phd[i].p_vaddr + phd[i].p_memsz){
            printf("Page fault at address is: %p\n", fa);
            PageMap(&phd[i], (char *)ehd, fa);
            PF++;
            return;
        }
    }
    signal(SIGSEGV, SIG_DFL);
    raise(SIGSEGV);
}


void LoadELF(char **exe){
    off_t DescSize;
    char *heap = ElfFileReader(*exe, &DescSize);
    ehd = (Elf32_Ehdr *)heap;
    phd = (Elf32_Phdr *)(heap + ehd->e_phoff);


    if(ehd->e_type!=ET_EXEC){
        printf("ELF file type is unsupported\n");
        free(heap);
        close(FileDesc);
        exit(1);
    }
    PageSize = sysconf(_SC_PAGESIZE);
    struct sigaction sigAc; //Setting up signal handler for lazy loading
    sigAc.sa_flags = SA_SIGINFO;
    sigAc.sa_sigaction = HandlePageFault;
    sigemptyset(&sigAc.sa_mask);

    if(sigaction(SIGSEGV, &sigAc, NULL) == -1){
        perror("Error: sigaction has failed!");
        free(heap);
        close(FileDesc);
        exit(1);
    }
    unsigned int entry;
    void *EntryAddress = EntryPointFinder(&entry);

    if(EntryAddress){
        printf("Entry point address is: %p\n", EntryAddress);
        int(*_start)(void) = (int (*)(void))EntryAddress;
        int answer = _start();
        printf("User _start return value = %d\n", answer);
    }else{
        printf("Entry Point Address is out of bounds.\n");
    }


    printf("Total page faults: %d\n", PF); //page faults and allocatin
    printf("Total page allocations: %d\n", PageAllocs);
    printf("Internal fragmentation: %zu KB\n", IntFrag/1024);
    free(heap);
    close(FileDesc);
    LoaderClean();
}


int main(int argc, char **argv){
    if(argc != 2){
        printf("Usage: %s <ELF Executable>\n", argv[0]);
        exit(1);
    }
    FILE *elfFile = fopen(argv[1], "rb");
    if(!elfFile){
        printf("Error: ELF file can't be opened!\n");
        exit(1);
    }

    fclose(elfFile);
    LoadELF(&argv[1]);
    return 0;
}