#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    unsigned char first_byte;
    unsigned char start_chs[3];
    unsigned char partition_type;
    unsigned char end_chs[3];
    unsigned long start_sector;
    unsigned long length_sectors;
} __attribute((packed)) PartitionTable;

typedef struct
{
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

typedef struct
{
    unsigned char filename[8];
    unsigned char ext[3];
    unsigned char attributes;
    unsigned char reserved[10];
    unsigned short modify_time;
    unsigned short modify_date;
    unsigned short starting_cluster;
    unsigned long file_size;
} __attribute((packed)) Fat16Entry;

void print_file_info(Fat16Entry *entry)
{
    switch (entry->filename[0])
    {
    case 0x00:
        return; // unused entry
    case 0xE5:
        printf("Deleted file: [?%.7s.%.3s]\n", entry->filename + 1, entry->ext);
        return;
    case 0x05:
        printf("File starting with 0xE5: [%c%.7s.%.3s]\n", 0xE5, entry->filename + 1, entry->ext);
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

void fat_read_file(FILE *in, FILE *out,
                   unsigned long fat_start,
                   unsigned long data_start,
                   unsigned long cluster_size,
                   unsigned short cluster,
                   unsigned long file_size)
{
    unsigned char buffer[4096];
    size_t bytes_read, bytes_to_read,
        file_left = file_size, cluster_left = cluster_size;

    // Go to first data cluster
    fseek(in, data_start + cluster_size * (cluster - 2), SEEK_SET);

    // Read until we run out of file or clusters
    while (file_left > 0 && cluster != 0xFFFF)
    {
        bytes_to_read = sizeof(buffer);

        // don't read past the file or cluster end
        if (bytes_to_read > file_left)
            bytes_to_read = file_left;
        if (bytes_to_read > cluster_left)
            bytes_to_read = cluster_left;

        // read data from cluster, write to file
        bytes_read = fread(buffer, 1, bytes_to_read, in);
        fwrite(buffer, 1, bytes_read, out);
        printf("Copied %d bytes\n", bytes_read);

        // decrease byte counters for current cluster and whole file
        cluster_left -= bytes_read;
        file_left -= bytes_read;

        // if we have read the whole cluster, read next cluster # from FAT
        if (cluster_left == 0)
        {
            fseek(in, fat_start + cluster * 2, SEEK_SET);
            fread(&cluster, 2, 1, in);

            printf("End of cluster reached, next cluster %d\n", cluster);

            fseek(in, data_start + cluster_size * (cluster - 2), SEEK_SET);
            cluster_left = cluster_size; // reset cluster byte counter
        }
    }
}

int main(int argc, char *argv[])
{
    // Read partition data
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

    // Read boot
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

    // Read data
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

    // Leer archivo de texto
    FILE *in, *out;
    int i, j;
    unsigned long fat_start, root_start, data_start;
    PartitionTable pt[4];
    Fat16BootSector bs;
    Fat16Entry entry;
    char filename[9] = "        ", file_ext[4] = "   "; // initially pad with spaces

    if (argc < 3)
    {
        printf("Usage: read_file <fs_image> <FILE.EXT>\n");
        return 0;
    }

    if ((in = fopen(argv[1], "rb")) == NULL)
    {
        printf("Filesystem image file %s not found!\n", argv[1]);
        return -1;
    }

    // Copy filename and extension to space-padded search strings
    for (i = 0; i < 8 && argv[2][i] != '.' && argv[2][i] != 0; i++)
        filename[i] = argv[2][i];
    for (j = 1; j <= 3 && argv[2][i + j] != 0; j++)
        file_ext[j - 1] = argv[2][i + j];

    printf("Opened %s, looking for [%s.%s]\n", argv[1], filename, file_ext);

    fseek(in, 0x1BE, SEEK_SET);               // go to partition table start
    fread(pt, sizeof(PartitionTable), 4, in); // read all four entries

    for (i = 0; i < 4; i++)
    {
        if (pt[i].partition_type == 4 || pt[i].partition_type == 6 ||
            pt[i].partition_type == 14)
        {
            printf("FAT16 filesystem found from partition %d\n", i);
            break;
        }
    }

    if (i == 4)
    {
        printf("No FAT16 filesystem found, exiting...\n");
        return -1;
    }

    fseek(in, 512 * pt[i].start_sector, SEEK_SET);
    fread(&bs, sizeof(Fat16BootSector), 1, in);

    // Calculate start offsets of FAT, root directory and data
    fat_start = ftell(in) + (bs.reserved_sectors - 1) * bs.sector_size;
    root_start = fat_start + bs.fat_size_sectors * bs.number_of_fats *
                                 bs.sector_size;
    data_start = root_start + bs.root_dir_entries * sizeof(Fat16Entry);

    printf("FAT start at %08X, root dir at %08X, data at %08X\n",
           fat_start, root_start, data_start);

    fseek(in, root_start, SEEK_SET);

    for (i = 0; i < bs.root_dir_entries; i++)
    {
        fread(&entry, sizeof(entry), 1, in);

        if (memcmp(entry.filename, filename, 8) == 0 &&
            memcmp(entry.ext, file_ext, 3) == 0)
        {
            printf("File found!\n");
            break;
        }
    }

    if (i == bs.root_dir_entries)
    {
        printf("File not found!");
        return -1;
    }

    out = fopen(argv[2], "wb"); // write the file contents to disk
    fat_read_file(in, out, fat_start, data_start, bs.sectors_per_cluster * bs.sector_size, entry.starting_cluster, entry.file_size);
    fclose(out);

    fclose(in);

    return 0;
}