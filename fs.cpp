#include <iostream>
#include "fs.h"
#include <string.h>

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";

    workingDirectory = 0;

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
    dir_entry all_entries[BLOCK_SIZE / 64];
    dir_entry root;

    std::string rootName = "/";
    strcpy(root.file_name, rootName.c_str()); // blev konstigt om jag satte namnet direkt till '/', ingen aning varför
    root.size = 0;
    root.first_blk = 0;
    root.type = TYPE_DIR;
    root.access_rights = 0x06; // Alla rättigheter

    // Array med alla entries i, vet ej om de är så men enda logiska enligt mig
    // När man ska ta reda på saker så loopar man i denna bara
    all_entries[0] = root;

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

    // Validera filepath
    // Lokalt?
    // Relativ?
    // Absolut?

    // Först kontrollera och godkänn namnet
    if (filepath.size() >= 56)
    {
        std::cout << "ERROR : Filename to large. Canceling file creation ..." << std::endl;
        return 0;
    }
    // HÄR MÅSTE VI KONTROLLERA OM ANNNAN FILL med samma namn finns!
    if (checkFileName(workingDirectory, filepath))
    {
        std::cout << "ERROR : A file with that name does already exist. Canceling file creation ..." << std::endl;
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

    // Omvandlar själva inputen till data som ska skrivas till disken
    // Nu skapar vi en direntry om filen, vet ej exakt vad namnet ska vara etc
    dir_entry currentFile = createDirEntry(filepath, completInput.size(), TYPE_FILE);
    // Nu ändrar VI FAT OCH DISK!
    writeToDisk(currentFile, completInput);

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

    // get contents of file how?
    // Ta reda på vilket start block
    dir_entry currentfile;
    dir_entry dirData[BLOCK_SIZE / 64];
    disk.read(workingDirectory, (uint8_t *)dirData);

    for (int i = 0; i < BLOCK_SIZE / 64; i++)
    {
        // då vet vi att vi hittat
        if (dirData[i].file_name == filepath)
        {
            currentfile = dirData[i];
            break;
        }
    }

    // Ta reda på vilka fat block filen finns i och returnera sedan.
    // Ta reda på amount of blocks.

    int amountOfBlocks;
    if (currentfile.size % BLOCK_SIZE == 0)
    {
        amountOfBlocks = currentfile.size / BLOCK_SIZE;
    }
    else
    {
        amountOfBlocks = (currentfile.size / BLOCK_SIZE) + 1;
    }

    int *fatFileBlocks;
    fatFileBlocks = fatFileIndex(currentfile.first_blk, amountOfBlocks);

    // DETTA MÅSTE FIXAS DÅ DE EJ TAR Hänsyn till filler över 4092 kb
    char data[currentfile.size];
    for (int i = 0; i < amountOfBlocks; i++)
    {
        disk.read(fatFileBlocks[i], (uint8_t *)data);
        std::cout << data << std::endl;
    }

    delete[] fatFileBlocks;

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls()
{
    std::cout << "FS::ls()\n";
    // LÄs från nuvarande directory
    dir_entry dirData[BLOCK_SIZE / 64];
    disk.read(workingDirectory, (uint8_t *)dirData);

    std::cout << "name\t";
    std::cout << "size" << std::endl;

    // nu har vi alla filler i denna, gå igenom dire och ta reda på vilka som existerar
    for (int i = 1; i < BLOCK_SIZE / 64; i++)
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
void FS::writeToDisk(dir_entry currentDir, std::string data)
{

    // Update fat
    int amountOfBlocks;
    int *freeFat;

    if (currentDir.size % BLOCK_SIZE == 0)
    {
        amountOfBlocks = currentDir.size / BLOCK_SIZE;
    }
    else
    {
        amountOfBlocks = (currentDir.size / BLOCK_SIZE) + 1;
    }

    if (amountOfBlocks > 1)
    {
        int tempArray[amountOfBlocks];
        freeFat = findFreeFatBlocks(currentDir.first_blk, amountOfBlocks, tempArray);

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

    dir_entry dirData[BLOCK_SIZE / 64];
    disk.read(workingDirectory, (uint8_t *)dirData);
    for (int i = 1; i < BLOCK_SIZE / 64; i++)
    {
        // 65535 är högsta value i unit16_t. Alla first.blk har blivit de som default
        //  Är firsdt_blk === 65535 vet vi att vi kan använda det då inget block i fat har index 65535.
        if (dirData[i].first_blk == 65535)
        {
            dirData[i] = currentDir;
            break;
        }
    }

    // Write to disk
    disk.write(ROOT_BLOCK, (uint8_t *)&dirData);

    // Gör om datan till char array
    char dataArr[data.size()];
    strcpy(dataArr, data.c_str());

    // Skriva in fil data till disk, vi vet platsenra, nu måste vi faktiskt göra det
    if (amountOfBlocks == 1)
    {
        disk.write(currentDir.first_blk, (uint8_t *)dataArr);
    }
    else
    {
        int firstIndex = 0;
        int lastIndex = BLOCK_SIZE;

        for (int i = 0; i < amountOfBlocks; i++)
        {

            if (i == (amountOfBlocks - 1))
            {
                // då vet vi att de är sista iteration.
                // Måste veta hur mycket som är kvar i bytes för arrayens skull.
                int bytesLeft = currentDir.size - firstIndex;
                char tempBuff[bytesLeft];
                std::copy(dataArr + firstIndex, dataArr + currentDir.size, tempBuff);
                disk.write(freeFat[i], (uint8_t *)tempBuff);
            }
            else
            {
                uint8_t tempBuff[BLOCK_SIZE];
                std::copy(dataArr + firstIndex, dataArr + lastIndex, tempBuff);
                firstIndex += BLOCK_SIZE;
                lastIndex += BLOCK_SIZE;
                disk.write(freeFat[i], (uint8_t *)tempBuff);
            }
        }
    }
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

int *FS::fatFileIndex(int firstblock, int amountOfBlocks)
{
    int *fatIndexArr = new int[100];
    int currentIndex = firstblock;

    for (int i = 0; i < amountOfBlocks; i++)
    {
        fatIndexArr[i] = currentIndex;
        currentIndex = fat[currentIndex];
    }

    return fatIndexArr;
}
