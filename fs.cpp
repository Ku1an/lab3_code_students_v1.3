#include <iostream>
#include "fs.h"
#include <string.h>

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";

    setWorkingDirectory(0);

    // Läser in fat och root, vet ej om vi behöver läsa root
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
    dir_entry currentDirectory[BLOCK_SIZE / sizeof(dir_entry)];
    dir_entry root = initDirEntry("/", 0, TYPE_DIR);
    root.first_blk = 0;

    // Array med alla entries i, vet ej om de är så men enda logiska enligt mig
    // När man ska ta reda på saker så loopar man i denna bara
    currentDirectory[0] = root;

    // Init the blocks in fat
    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;
    for (int i = 2; i < BLOCK_SIZE / 2; i++)
    {
        fat[i] = FAT_FREE;
    }

    // Skriv till diksen
    disk.write(ROOT_BLOCK, (uint8_t *)&currentDirectory);
    disk.write(FAT_BLOCK, (uint8_t *)&fat);

    // Vår working directory
    setWorkingDirectory(0);

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";

    // Fil kontroll godkänn filen.
    if (filepath.size() >= 56)
    {
        std::cout << "ERROR : Filename to large. Canceling file creation ..." << std::endl;
        return 0;
    }
    if (checkFileName(workingDirectory, filepath))
    {
        std::cout << "ERROR : A file with that name does already exist. Canceling file creation ..." << std::endl;
        return 0;
    }

    if (!checkCwdSpace())
    {
        std::cout << "ERROR : No place in current directory. Canceling file creation ..." << std::endl;
        return 0;
    }

    // Kontrollera om de finns space i nuvarnde dir
    // Läsa input från användaren.
    std::string completeInput;
    std::string userInput;
    while (getline(std::cin, userInput))
    {
        if (userInput.empty())
        {
            break;
        }
        completeInput += userInput + "\n";
    }
    // Skapar en Directory entry för filen.
    dir_entry newFile = initDirEntry(filepath, completeInput.size(), TYPE_FILE);

    // updateFat, uppdaterar enbart fat, om vart filen ska vara osv.
    diskWrite(newFile, completeInput);
    updateFat(newFile);
    updateDiskDirEntry(newFile);

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    // kolla först namnet
    if (filepath.size() >= 56 || !checkFileName(workingDirectory, filepath))
    {
        std::cout << "ERROR : File does not exist. Canceling cat file ..." << std::endl;
        return 0;
    }

    // Först få filens dir_entry
    dir_entry currentfile = getFileDirEntry(filepath);
    std::string output = "";

    if (currentfile.size < BLOCK_SIZE)
    {
        char data[BLOCK_SIZE];
        disk.read(currentfile.first_blk, (uint8_t *)data);
        output = data;
    }
    else
    {
        // ger oss en array på alla index som fat, sedan adderar vi i output
        int *dataBlocks;
        dataBlocks = getFileBlockLocation(currentfile.first_blk);

        for (int i = 0; i < BLOCK_SIZE / 2; i++)
        {
            if (dataBlocks[i] == -1)
            {
                break;
            }
            char data[BLOCK_SIZE];
            disk.read(dataBlocks[i], (uint8_t *)data);
            output += data;
        }
        delete[] dataBlocks;
    }
    std::cout << output << std::endl;
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls()
{
    std::cout << "FS::ls()\n";
    // LÄs från nuvarande directory
    dir_entry dirData[BLOCK_SIZE / 64];
    disk.read(getWorkingDirectory(), (uint8_t *)dirData);

    std::cout << "name\t";
    std::cout << "size" << std::endl;

    // nu har vi alla filler i denna, gå igenom dire och ta reda på vilka som existerar
    for (int i = 1; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (dirData[i].first_blk != 65535)
        {
            // då vet vi att de är en fil
            std::cout << dirData[i].file_name << "\t" << dirData[i].size << std::endl;
        }
    }

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
    dir_entry currentCwd[BLOCK_SIZE / 64];
    getCurrentWorkDirectory(currentCwd);
    std::cout << currentCwd->file_name << std::endl;

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
dir_entry FS::initDirEntry(std::string name, uint32_t sizeOfFile, uint8_t fileType)
{
    dir_entry currentDir;
    strcpy(currentDir.file_name, name.c_str());
    currentDir.size = sizeOfFile;
    currentDir.first_blk = getFirstFreeFatBlock(fat);
    currentDir.type = fileType;
    currentDir.access_rights = 0x06;

    return currentDir;
}

// This function does changes in FAT when a new file takes place, it also returns the first block of file
// Kanske ändra i fatten, samt i disken????

int FS::getFirstFreeFatBlock(int16_t fatTable[BLOCK_SIZE / 2])
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

// Funktionen returnerar en dynamsik array som innehåller de block, kom ihåg delete!!!
int *FS::getFreeFatSlots(int firstBlock, int amountOfBlocks)
{
    int currentIndex = 1;
    int *fatSlots = new int[amountOfBlocks];
    fatSlots[0] = firstBlock;

    for (int i = firstBlock + 1; i < DISKBLOCKS; i++)
    {
        if (fat[i] == FAT_FREE)
        {
            fatSlots[currentIndex] = i;
            currentIndex++;
        }

        // Found all free blocks needed for file
        if (currentIndex == amountOfBlocks)
        {
            break;
        }
    }
    return fatSlots;
}

int *FS::getFileBlockLocation(int firstblock)
{

    // Borde loopa i fat för att se vart filen befinner sig
    int *fatIndexArr = new int[BLOCK_SIZE / 2];
    fatIndexArr[0] = firstblock;
    int currentindex = firstblock;
    int loopCounter = 1;

    bool fat_eof = false;
    while (!fat_eof)
    {
        if (currentindex != FAT_EOF)
        {
            fatIndexArr[loopCounter] = fat[currentindex];
            currentindex = fat[currentindex];
            loopCounter++;
        }
        else
        {
            fat_eof = true;
        }
    }

    return fatIndexArr;
}

// Funktionen basically räknar ut hur många blocks en given size kommer delsa in i på disken.
//  Detta då varje block enbart har utrymme för  4096 bytes
int FS::getAmountOfBlocks(int size)
{
    int amountOfBlocks;
    if (size % BLOCK_SIZE == 0)
    {
        if (size != 0)
        {
            amountOfBlocks = size / BLOCK_SIZE;
        }
        else
        {
            amountOfBlocks = 1;
        }
    }
    else
    {
        amountOfBlocks = (size / BLOCK_SIZE) + 1;
    }
    return amountOfBlocks;
}

dir_entry FS::getFileDirEntry(std::string fileName)
{
    dir_entry file;
    dir_entry dirData[BLOCK_SIZE / 64];
    disk.read(getWorkingDirectory(), (uint8_t *)dirData);

    for (int i = 0; i < BLOCK_SIZE / 64; i++)
    {
        // då vet vi att vi hittat
        if (dirData[i].file_name == fileName)
        {
            file = dirData[i];
            break;
        }
    }
    return file;
}
void FS::getCurrentWorkDirectory(dir_entry *currentWorkDir)
{
    dir_entry dirData[BLOCK_SIZE / 64];
    disk.read(getWorkingDirectory(), (uint8_t *)dirData);
    memcpy(currentWorkDir, dirData, sizeof(dir_entry));
}

// Returnerar sant om ett filnamn finns,
bool FS::checkFileName(int currentWorkDir, std::string filename)
{
    bool nameExist = false;

    // Läs från disk o få data.
    dir_entry dirData[BLOCK_SIZE / 64];
    disk.read(currentWorkDir, (uint8_t *)dirData);

    for (int i = 0; i < BLOCK_SIZE / 64; i++)
    {
        if (dirData[i].first_blk != 65535 && dirData[i].file_name == filename)
        {
            nameExist = true;
            break;
        }
    }

    return nameExist;
}

bool FS::checkCwdSpace()
{
    bool cwdSpace = false;

    dir_entry dirData[BLOCK_SIZE / 64];
    disk.read(getWorkingDirectory(), (uint8_t *)dirData);

    for (int i = 1; i < BLOCK_SIZE / 64; i++)
    {
        // 65535 är högsta value i unit16_t. Alla first.blk har blivit de som default
        //  Är firsdt_blk === 65535 vet vi att vi kan använda det då inget block i fat har index 65535.
        // Om ej vet vi att ingen plats existerar;
        if (dirData[i].first_blk == 65535)
        {
            cwdSpace = true;
            break;
        }
    }
    return cwdSpace;
}

void FS::updateDiskDirEntry(dir_entry newFile)
{
    dir_entry dirData[BLOCK_SIZE / 64];
    disk.read(getWorkingDirectory(), (uint8_t *)dirData);
    for (int i = 1; i < BLOCK_SIZE / 64; i++)
    {
        // 65535 är högsta value i unit16_t. Alla first.blk har blivit de som default
        //  Är firsdt_blk === 65535 vet vi att vi kan använda det då inget block i fat har index 65535.
        if (dirData[i].first_blk == 65535)
        {
            dirData[i] = newFile;
            break;
        }
    }
    disk.write(getWorkingDirectory(), (uint8_t *)&dirData);
}

void FS::updateFat(dir_entry currentFile)
{
    // Update fat and Disk
    int amountOfBlocks = getAmountOfBlocks(currentFile.size);
    if (amountOfBlocks == 1)
    {
        fat[currentFile.first_blk] = FAT_EOF;
    }
    else
    {
        int *freeFatSlots = getFreeFatSlots(currentFile.first_blk, amountOfBlocks);

        for (int i = 0; i < amountOfBlocks - 1; i++)
        {
            fat[freeFatSlots[i]] = freeFatSlots[i + 1];
        }

        fat[freeFatSlots[amountOfBlocks - 1]] = FAT_EOF;
        delete[] freeFatSlots;
    }
    disk.write(FAT_BLOCK, (uint8_t *)&fat);
}

void FS::diskWrite(dir_entry currentDir, std::string data)
{

    int amountOfBlocks = getAmountOfBlocks(currentDir.size);
    // Gör om datan till char array
    char dataArr[data.size()];
    strcpy(dataArr, data.c_str());

    // Nu skriver vi
    if (amountOfBlocks == 1)
    {
        disk.write(currentDir.first_blk, (uint8_t *)dataArr);
    }
    else
    {
        int firstIndex = 0;
        int lastIndex = BLOCK_SIZE;
        int *freeFatSlots = getFreeFatSlots(currentDir.first_blk, amountOfBlocks);

        for (int i = 0; i < amountOfBlocks; i++)
        {

            if (i == (amountOfBlocks - 1))
            {
                // då vet vi att de är sista iteration.
                // Måste veta hur mycket som är kvar i bytes för arrayens skull.
                int bytesLeft = currentDir.size - firstIndex;
                char tempBuff[bytesLeft];
                std::copy(dataArr + firstIndex, dataArr + currentDir.size, tempBuff);
                disk.write(freeFatSlots[i], (uint8_t *)tempBuff);
            }
            else
            {
                uint8_t tempBuff[BLOCK_SIZE];
                std::copy(dataArr + firstIndex, dataArr + lastIndex, tempBuff);
                firstIndex += BLOCK_SIZE;
                lastIndex += BLOCK_SIZE;
                disk.write(freeFatSlots[i], (uint8_t *)tempBuff);
            }
        }
        delete[] freeFatSlots;
    }
}
