#include <stdio.h>
#include <stdlib.h>

typedef struct {
    unsigned char first_byte;
    unsigned char start_chs[3];
    unsigned char partition_type;
    unsigned char end_chs[3];
    unsigned long start_sector;
    unsigned long length_sectors;
} __attribute((packed)) PartitionTable;

typedef struct {
    unsigned char jmp[3];
    char oem[8];
    unsigned short sector_size;
    unsigned char sectors_per_cluster;
    unsigned short reserved_sectors;
    unsigned char number_of_fats;
    unsigned short root_dir_entries;
    unsigned short total_sectors_short; // if zero, later field is used
    unsigned char media_descriptor;
    unsigned short fat_size_sectors;
    unsigned short sectors_per_track;
    unsigned short number_of_heads;
    unsigned long hidden_sectors;
    unsigned long total_sectors_long;
    
    unsigned char drive_number;
    unsigned char current_head;
    unsigned char boot_signature;
    unsigned long volume_id;
    char volume_label[11];
    char fs_type[8];
    char boot_code[448];
    unsigned short boot_sector_signature;
} __attribute((packed)) Fat16BootSector;

typedef struct {
    unsigned char filename[8];
    unsigned char ext[3];
    unsigned char attributes;
    unsigned char reserved[10];
    unsigned short modify_time;
    unsigned short modify_date;
    unsigned short starting_cluster;
    unsigned long file_size;
} __attribute((packed)) Fat16Entry;

void print_file_info(Fat16Entry *entry) {
    switch(entry->filename[0]) {
    case 0x00:
        return; // unused entry
    case 0xE5:
        printf("Deleted file: [?%.7s.%.3s]\n", entry->filename+1, entry->ext);
        return;
    case 0x05:
        printf("File starting with 0xE5: [%c%.7s.%.3s]\n", 0xE5, entry->filename+1, entry->ext);
        break;
    case 0x2E:
        printf("Directory: [%.8s.%.3s]\n", entry->filename, entry->ext);
        break;
    default:
        printf("File: [%.8s.%.3s]\n", entry->filename, entry->ext);
    }
    
    printf("  Modified: %04d-%02d-%02d %02d:%02d.%02d    Start: [%04X]    Size: %d\n", 
        1980 + (entry->modify_date >> 9), (entry->modify_date >> 5) & 0xF, entry->modify_date & 0x1F,
        (entry->modify_time >> 11), (entry->modify_time >> 5) & 0x3F, entry->modify_time & 0x1F,
        entry->starting_cluster, entry->file_size);
}

int main() {
    //Read partition data
    /*FILE * in = fopen("test.img", "rb");
    int i;
    PartitionTable pt[4];
    
    fseek(in, 0x1BE, SEEK_SET); // go to partition table start
    fread(pt, sizeof(PartitionTable), 4, in); // read all four entries
    
    for(i=0; i<4; i++) {
        printf("Partition %d, type %02X\n", i, pt[i].partition_type);
        printf("  Start sector %08X, %d sectors long\n", 
                pt[i].start_sector, pt[i].length_sectors);
    }
    
    fclose(in);
    return 0;*/

    //Read boot
    /*FILE * in = fopen("test.img", "rb");
    int i;
    PartitionTable pt[4];
    Fat16BootSector bs;
    
    fseek(in, 0x1BE, SEEK_SET); // go to partition table start
    fread(pt, sizeof(PartitionTable), 4, in); // read all four entries
    
    for(i=0; i<4; i++) {        
        if(pt[i].partition_type == 4 || pt[i].partition_type == 6 ||
           pt[i].partition_type == 14) {
            printf("FAT16 filesystem found from partition %d\n", i);
            break;
        }
    }
    
    if(i == 4) {
        printf("No FAT16 filesystem found, exiting...\n");
        return -1;
    }
    
    fseek(in, 512 * pt[i].start_sector, SEEK_SET);
    fread(&bs, sizeof(Fat16BootSector), 1, in);
    
    printf("  Jump code: %02X:%02X:%02X\n", bs.jmp[0], bs.jmp[1], bs.jmp[2]);
    printf("  OEM code: [%.8s]\n", bs.oem);
    printf("  sector_size: %d\n", bs.sector_size);
    printf("  sectors_per_cluster: %d\n", bs.sectors_per_cluster);
    printf("  reserved_sectors: %d\n", bs.reserved_sectors);
    printf("  number_of_fats: %d\n", bs.number_of_fats);
    printf("  root_dir_entries: %d\n", bs.root_dir_entries);
    printf("  total_sectors_short: %d\n", bs.total_sectors_short);
    printf("  media_descriptor: 0x%02X\n", bs.media_descriptor);
    printf("  fat_size_sectors: %d\n", bs.fat_size_sectors);
    printf("  sectors_per_track: %d\n", bs.sectors_per_track);
    printf("  number_of_heads: %d\n", bs.number_of_heads);
    printf("  hidden_sectors: %d\n", bs.hidden_sectors);
    printf("  total_sectors_long: %d\n", bs.total_sectors_long);
    printf("  drive_number: 0x%02X\n", bs.drive_number);
    printf("  current_head: 0x%02X\n", bs.current_head);
    printf("  boot_signature: 0x%02X\n", bs.boot_signature);
    printf("  volume_id: 0x%08X\n", bs.volume_id);
    printf("  Volume label: [%.11s]\n", bs.volume_label);
    printf("  Filesystem type: [%.8s]\n", bs.fs_type);
    printf("  Boot sector signature: 0x%04X\n", bs.boot_sector_signature);
    
    fclose(in);
    return 0;*/

    //Read data
    /*FILE * in = fopen("test.img", "rb");
    int i;
    PartitionTable pt[4];
    Fat16BootSector bs;
    Fat16Entry entry;
    
    fseek(in, 0x1BE, SEEK_SET); // go to partition table start
    fread(pt, sizeof(PartitionTable), 4, in); // read all four entries
    
    for(i=0; i<4; i++) {        
        if(pt[i].partition_type == 4 || pt[i].partition_type == 6 ||
           pt[i].partition_type == 14) {
            printf("FAT16 filesystem found from partition %d\n", i);
            break;
        }
    }
    
    if(i == 4) {
        printf("No FAT16 filesystem found, exiting...\n");
        return -1;
    }
    
    fseek(in, 512 * pt[i].start_sector, SEEK_SET);
    fread(&bs, sizeof(Fat16BootSector), 1, in);
    
    printf("Now at 0x%X, sector size %d, FAT size %d sectors, %d FATs\n\n", 
           ftell(in), bs.sector_size, bs.fat_size_sectors, bs.number_of_fats);
           
    fseek(in, (bs.reserved_sectors-1 + bs.fat_size_sectors * bs.number_of_fats) *
          bs.sector_size, SEEK_CUR);
        
    for(i=0; i<bs.root_dir_entries; i++) {
        fread(&entry, sizeof(entry), 1, in);
        print_file_info(&entry);
    }
    
    printf("\nRoot directory read, now at 0x%X\n", ftell(in));
    fclose(in);
    return 0;*/
}