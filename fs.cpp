#include <iostream>
#include "fs.h"
#include <string.h>

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";

    workingDirectory = 0;

    // Läser in fat och root
    disk.read(ROOT_BLOCK, (uint8_t *)&all_entries);
    disk.read(FAT_BLOCK, (uint8_t *)&fat);
}

FS::~FS()
{
    // Skriva in fat och root datan om programmet stängs?
}

// formats the disk, i.e., creates an empty file system
int FS::format()
{
    std::cout << "FS::format()\n";

    // Skapar root entry i disken
    dir_entry root;
    root.file_name[0] = '/';
    root.size = 0;
    root.first_blk = 0;
    root.type = TYPE_DIR;
    root.access_rights = 0x06; // Alla rättigheter

    // Array med alla entries i, vet ej om de är så men enda logiska enligt mig
    // När man ska ta reda på saker så loopar man i denna bara
    all_entries[0] = root;

    for (int i = 1; i < DISKBLOCKS; i++)
    {
        // Kommer hjälpa oss när vi ska storea saker i arrayen, är first_blk = -1 existerar ingen entry på den plats.
        all_entries[i].first_blk = -1;
    }

    // Init the blocks in fat
    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;
    for (int i = 2; i < BLOCK_SIZE / 2; i++)
    {
        fat[i] = FAT_FREE;
    }

    // Skriv till diksen
    disk.write(ROOT_BLOCK, (uint8_t *)&all_entries);
    disk.write(FAT_BLOCK, (uint8_t *)&fat);

    // Vår working directory
    workingDirectory = 0;

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";

    // Först kontrollera och godkänn namnet
    if (filepath.size() > 56)
    {
        std::cout << "ERROR : Filename to large. Canceling file creation ...";
        return 0;
    }

    // Läsa input
    bool input = true;
    std::string completInput;
    std::string userInput;
    while (input)
    {
        getline(std::cin, userInput);

        if (userInput != "")
        {
            completInput = completInput + userInput;
        }
        else
        {
            input = false;
        }
    }

    // Nu skapar vi en direntry om filen, vet ej exakt vad namnet ska vara etc
    dir_entry currentFile = createDirEntry(filepath, completInput.size(), TYPE_FILE);
    // Nu ändrar VI FAT OCH DISK!
    writeToDisk(currentFile);

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls()
{
    std::cout << "FS::ls()\n";
    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}

// Own written code
// Skapar en dir_entry, möjligen ska denna ändras för att ha koll på subdirectroies och vart filler befinner sig etc
dir_entry FS::createDirEntry(std::string name, uint32_t sizeOfFile, uint8_t fileType)
{
    dir_entry currentDir;
    strcpy(currentDir.file_name, name.c_str());
    currentDir.size = sizeOfFile;
    currentDir.first_blk = firstEmptyFatBlock(fat);
    currentDir.type = fileType;
    currentDir.access_rights = 0x06;

    return currentDir;
}

// This function does changes in FAT when a new file takes place, it also returns the first block of file
// Kanske ändra i fatten, samt i disken????

int FS::firstEmptyFatBlock(int16_t fatTable[BLOCK_SIZE / 2])
{
    // Sätter minus ett, om de inte hittar vet vi om det.
    //  Det betyder att faten är full. Om de inte går ska de givetvis hanteras.
    int first_empty_index = -1;

    for (int i = 2; i < DISKBLOCKS; i++)
    {
        // Hittar vi slutar vi leta.
        if (fatTable[i] == FAT_FREE)
        {
            first_empty_index = i;
            break;
        }
    }
    return first_empty_index;
}

int *FS::findFreeFatBlocks(int firstBlock, int amountOfBlocks, int arr[])
{
    int currentIndex = 1;
    arr[0] = firstBlock;

    for (int i = firstBlock + 1; i < DISKBLOCKS; i++)
    {
        if (fat[i] == FAT_FREE)
        {
            arr[currentIndex] = i;
            currentIndex++;
        }

        // Found all free blocks needed for file
        if (currentIndex == amountOfBlocks)
        {
            break;
        }
    }
    return arr;
}

// This function writes the new file to disk and also updates fat and all_entries the
void FS::writeToDisk(dir_entry currentDir)
{
    // Update fat
    int amountOfBlocks;

    if (currentDir.size % BLOCK_SIZE == 0)
    {
        amountOfBlocks = currentDir.size / BLOCK_SIZE;
    }
    else
    {
        amountOfBlocks = (currentDir.size / BLOCK_SIZE) + 1;
    }

    // test
    /*amountOfBlocks = 2;
    fat[2] = FAT_EOF;
    fat[3] = FAT_EOF;
    fat[5] = FAT_EOF;
    fat[6] = FAT_EOF;
    fat[9] = FAT_EOF;
    currentDir.first_blk = 4;*/

    // Now find amountOfBlocks slots
    if (amountOfBlocks > 1)
    {
        int tempArray[amountOfBlocks];
        int *freeFat = findFreeFatBlocks(currentDir.first_blk, amountOfBlocks, tempArray);

        // Then change in fat here
        for (int i = 0; i < amountOfBlocks - 1; i++)
        {
            fat[freeFat[i]] = freeFat[i + 1];
        }
        fat[freeFat[amountOfBlocks - 1]] = FAT_EOF;
    }
    else
    {
        fat[currentDir.first_blk] = FAT_EOF;
    }

    // Add to entry
    for (int i = 1; i < DISKBLOCKS; i++)
    {
        if (all_entries[i].first_blk == -1)
        {
            all_entries[i] = currentDir;
            break;
        }
    }

    // Write to disk
    disk.write(ROOT_BLOCK, (uint8_t *)&all_entries);

    // Lägg till datan sen här
}