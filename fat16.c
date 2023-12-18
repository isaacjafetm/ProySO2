#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#define min(a, b) ((a) < (b) ? (a) : (b))

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

void s()
{
    FILE *in = fopen("test.img", "rb");
    int i;
    PartitionTable pt[4];
    Fat16BootSector bs;
    Fat16Entry entry;

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

    printf("Now at 0x%X, sector size %d, FAT size %d sectors, %d FATs\n\n",
           ftell(in), bs.sector_size, bs.fat_size_sectors, bs.number_of_fats);

    fseek(in, (bs.reserved_sectors - 1 + bs.fat_size_sectors * bs.number_of_fats) * bs.sector_size, SEEK_CUR);

    for (i = 0; i < bs.root_dir_entries; i++)
    {
        fread(&entry, sizeof(entry), 1, in);
        print_file_info(&entry);
    }

    printf("\nRoot directory read, now at 0x%X\n", ftell(in));
    fclose(in);
}

void cat(const char *txt)
{
    const char *img = "test.img";
    FILE *in, *out;
    int i, j;
    unsigned long fat_start, root_start, data_start;
    PartitionTable pt[4];
    Fat16BootSector bs;
    Fat16Entry entry;
    char filename[9] = "        ", file_ext[4] = "   "; // initially pad with spaces

    /*if (argc < 3)
    {
        printf("Usage: read_file <fs_image> <FILE.EXT>\n");
        return 0;
    }*/

    if ((in = fopen(img, "rb")) == NULL)
    {
        printf("Filesystem image file %s not found!\n", img);
        return -1;
    }

    // Copy filename and extension to space-padded search strings
    for (i = 0; i < 8 && txt[i] != '.' && txt[i] != 0; i++)
        filename[i] = txt[i];
    for (j = 1; j <= 3 && txt[i + j] != 0; j++)
        file_ext[j - 1] = txt[i + j];

    printf("Opened %s, looking for [%s.%s]\n", img, filename, file_ext);

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

    out = fopen(txt, "wb"); // write the file contents to disk
    fat_read_file(in, out, fat_start, data_start, bs.sectors_per_cluster * bs.sector_size, entry.starting_cluster, entry.file_size);
    fclose(out);

    fclose(in);
}

long calculateRootDirOffset(const Fat16BootSector *bs) {
    // Calculate the total size of reserved area including all FATs
    uint32_t reserved_size = bs->reserved_sectors + bs->number_of_fats * bs->fat_size_sectors;

    // Calculate the size of the root directory area in bytes
    uint32_t root_dir_size = bs->root_dir_entries * sizeof(Fat16Entry);

    // Calculate the offset of the root directory area
    return reserved_size * bs->sector_size;
}

void cd(const char *imageFilePath, const char *dirName) {
    FILE *imageFile = fopen(imageFilePath, "rb");
    if (!imageFile) {
        fprintf(stderr, "Error: Unable to open image file.\n");
        return;
    }
        int dirHistoryIndex = 0;

    FILE *in = fopen("test.img", "rb");
    int i;
    PartitionTable pt[4];
    Fat16BootSector bs;
    uint16_t currentDirCluster = 0;                // Initialize with the appropriate value
    char currentPath[256] = "";                    // Initialize with the appropriate value
    uint16_t dirHistory[256] = {0};                // Initialize with the appropriate value
    long rootDirOffset = calculateRootDirOffset(&bs); // Implement calculateRootDirOffset function


    // Fat16Entry entry;

    if (strcmp(dirName, "..") == 0) {
        if (!dirHistoryIndex) {
            fprintf(stderr, "Ya estás en el directorio raíz.\n");
            return;
        }
        else {
            currentDirCluster = dirHistory[dirHistoryIndex - 1];
            dirHistoryIndex--;
            return;
        }
    }

    uint32_t fat_start = rootDirOffset - bs.root_dir_entries * sizeof(Fat16Entry);
    uint32_t data_start = rootDirOffset + (bs.root_dir_entries * sizeof(Fat16Entry));
    uint32_t cluster_size = bs.sectors_per_cluster * bs.sector_size;

    fseek(in, rootDirOffset, SEEK_SET);
    Fat16Entry entry;
    char dirname[9] = "        ";

    char upperDirName[256];
    strcpy(upperDirName, dirName);
    for (int i = 0; upperDirName[i]; i++) {
        upperDirName[i] = toupper(upperDirName[i]);
    }
    strncpy(dirname, upperDirName, fmin(8, strlen(upperDirName)));

    for (int i = 0; i < bs.root_dir_entries; i++) {
        fread(&entry, sizeof(entry), 1, in);
        if (memcmp(entry.filename, dirname, 8) == 0 && (entry.attributes & 0x10)) {
            // Encontrado el directorio
            dirHistory[dirHistoryIndex] = currentDirCluster;
            dirHistoryIndex++;
            currentDirCluster = entry.starting_cluster;
            strcpy(currentPath, dirName);
            return;
        }
    }

    fprintf(stderr, "Directorio no encontrado: %s\n", dirName);
}

int main(int argc, char *argv[])
{
    // Read data
    if (strcmp(argv[1], "s") == 0)
    {
        s();
    }
    else if (strcmp(argv[1], "cat") == 0 && strcmp(argv[2], "a.txt") != 0)
    {
        cat(argv[2]);
    }
    else if (strcmp(argv[1], "mkdir") == 0)
    {
    }
    else if (strcmp(argv[1], "cat") == 0 && strcmp(argv[2], "a.txt") == 0)
    {
    }
    else if (strcmp(argv[1], "cd") == 0)
    {
        
    }

    // Leer archivo de texto

    return 0;
}