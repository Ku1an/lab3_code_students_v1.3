#include <iostream>
#include <cstdint>
#include "disk.h"
#include <vector>
#include <regex>
#include <string.h>
#include <sstream>

#ifndef __FS_H__
#define __FS_H__

#define ROOT_BLOCK 0
#define FAT_BLOCK 1
#define FAT_FREE 0
#define FAT_EOF -1

#define TYPE_FILE 0
#define TYPE_DIR 1
#define READ 0x04
#define WRITE 0x02
#define EXECUTE 0x01

#define UPDATE 2
#define CREATE 1
#define DELETE 0

struct dir_entry
{
    char file_name[56] = "";         // name of the file / sub-directory, hände något skumt här därför = ""
    uint32_t size;              // size of the file in bytes
    uint16_t first_blk = 65535; // index in the FAT for the first block of the file, Ger startvärde då det kommer underlätta
    uint8_t type;               // directory (1) or file (0)
    uint8_t access_rights;      // read (0x04), write (0x02), execute (0x01)
};

class FS
{
private:
    Disk disk;
    // size of a FAT entry is 2 bytes
    int16_t fat[BLOCK_SIZE / 2];
    int workingDirectory;

    // Viktiga
    void setWorkingDirectory(int newWorkingDirectory) { workingDirectory = newWorkingDirectory; }
    int getWorkingDirectory() { return workingDirectory; }

    dir_entry initDirEntry(std::string name, uint32_t sizeOfFile, uint8_t fileType, uint8_t accessright = 0x06);
    // Setters
    // Getters, användbara funktioner. Kommer användas i många olika
    // Returnerar given mängd block en size ska delas in i
    int getAmountOfBlocks(int size);
    // Returnerar de slots i faten som är lediga för filen. Dvs, om filen ska delas in i 3 delar returneras 3 lediga fat index.
    int *getFreeFatSlots(int firstBlock, int amountOfBlocks);
    // Returnerar första lediga index i fat.
    int getFirstFreeFatBlock(int16_t fat[BLOCK_SIZE / 2]);

    // Returnerar en array av de index i disken som filen har sin data i från fat.
    int *getFileBlockLocation(int firstblock);
    // Returnerar en fils data, exempelvis textn i,
    std::string getFileData(dir_entry file);
    // Sätter given array av dir_entrys till data som består av de nuvarande.
    void getCurrentWorkDirectoryEntries(dir_entry *currentWorkDir, int workdirectory);
    // Returnerar en fils dir_entry
    dir_entry getFileDirEntry(std::string fileName, int cwd);
    void setAccessRights(dir_entry *file, int accessRights);
    std::string getAccessRights(dir_entry file);
    // Returnar en vektor med all data i pathen.
    std::vector<std::string> getAbsoluteFilepath(std::string filepath);
    // Retunerar den work directory som kommer vara intressant, om -1 funkar de inte helt enkelt
    int testAbsoluteFilepath(std::vector<std::string> filepathcontents, bool type);

    // Returnerar en string av nuvarande pwd
    std::string getPwd();
    // Skriver til disken
    void diskWrite(dir_entry currentFile, std::string data);
    // Updaterar fat, används när vi skrivit något till disken , eller raderat, beror på!
    void updateFat(dir_entry currentFile, int deleteOrCreate);
    // Uppdaterar nuvarnde working directory dir_entryies
    void updateDiskDirEntry(dir_entry newFile, int deleteOrCreate, int cwd);
    // Kontrollera om ett namn är giltigt för filen.
    bool checkFileName(int currentWorkDir, std::string filename);
    // Kontrollerar om en given directory är tom eller ej
    bool checkEmptyDirectory(dir_entry dirname);
    // Kontrollerar om det finns utrymme för fil i nuvarande workingdirectory.
    bool checkCwdSpace(int cwd);

public:
    FS();
    ~FS();
    // formats the disk, i.e., creates an empty file system
    int format();
    // create <filepath> creates a new file on the disk, the data content is
    // written on the following rows (ended with an empty row)
    int create(std::string filepath);
    int create(std::string filepath, std::string input, int tempWorkDir, uint8_t accessright = 0x06);
    // cat <filepath> reads the content of a file and prints it on the screen
    int cat(std::string filepath);
    // ls lists the content in the current directory (files and sub-directories)
    int ls();

    // cp <sourcepath> <destpath> makes an exact copy of the file
    // <sourcepath> to a new file <destpath>
    int cp(std::string sourcepath, std::string destpath);
    // mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
    // or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
    int mv(std::string sourcepath, std::string destpath);
    // rm <filepath> removes / deletes the file <filepath>
    int rm(std::string filepath);
    // append <filepath1> <filepath2> appends the contents of file <filepath1> to
    // the end of file <filepath2>. The file <filepath1> is unchanged.
    int append(std::string filepath1, std::string filepath2);

    // mkdir <dirpath> creates a new sub-directory with the name <dirpath>
    // in the current directory
    int mkdir(std::string dirpath);
    // cd <dirpath> changes the current (working) directory to the directory named <dirpath>
    int cd(std::string dirpath);
    // pwd prints the full path, i.e., from the root directory, to the current
    // directory, including the current directory name
    int pwd();

    // chmod <accessrights> <filepath> changes the access rights for the
    // file <filepath> to <accessrights>.
    int chmod(std::string accessrights, std::string filepath);
};

#endif // __FS_H__
