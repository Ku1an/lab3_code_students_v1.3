#include "fs.h"

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
    if (checkFileName(getWorkingDirectory(), filepath))
    {
        std::cout << "ERROR : A file/dir with that name does already exist. Canceling file creation ..." << std::endl;
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
    updateFat(newFile, CREATE);
    updateDiskDirEntry(newFile, CREATE, getWorkingDirectory());

    return 0;
}

int FS::create(std::string filepath, std::string input, int tempWorkDir)
{
    dir_entry newFile = initDirEntry(filepath, input.size(), TYPE_FILE);

    diskWrite(newFile, input);
    updateFat(newFile, CREATE);
    updateDiskDirEntry(newFile, CREATE, tempWorkDir);

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    // kolla först namnet
    if (filepath.size() >= 56 || !checkFileName(getWorkingDirectory(), filepath))
    {
        std::cout << "ERROR : File does not exist. Canceling cat file ..." << std::endl;
        return 0;
    }

    // Först få filens dir_entry
    dir_entry currentfile = getFileDirEntry(filepath);

    if (currentfile.type == TYPE_DIR)
    {
        std::cout << "ERROR : You cannot read a directory. Canceling cat file ..." << std::endl;
        return 0;
    }
    std::string output = getFileData(currentfile);
    std::cout << output << std::endl;
    return 0;
}

// ls lists the content in the current directory (files and sub-directories)
int FS::ls()
{
    std::cout << "FS::ls()\n";
    // LÄs från nuvarande directory
    dir_entry currentCwd[BLOCK_SIZE / 64];
    getCurrentWorkDirectoryEntries(currentCwd, getWorkingDirectory());

    std::cout << "name\t";
    std::cout << "type\t";
    std::cout << "size" << std::endl;

    // nu har vi alla filler i denna, gå igenom dire och ta reda på vilka som existerar
    // I Sub directorys finns de parent_entry, vi vill inte skriv ut de till användaren
    int startIndex = 0;
    std::string name = currentCwd->file_name;
    if (name == "..")
    {
        startIndex += 1;
    }

    for (int i = startIndex; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (currentCwd[i].first_blk != 65535)
        {
            // då vet vi att de är giltig entry
            std::string type = "file";
            if (currentCwd[i].type)
            {
                type = "dir";
            }

            std::string size = std::to_string(currentCwd[i].size);
            if (type == "dir")
            {
                size = "-";
            }
            std::cout << currentCwd[i].file_name << "\t" << type << "\t" << size << std::endl;
        }
    }

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";

    if (destpath.size() >= 56)
    {
        std::cout << "ERROR : Filename to large. Canceling copy operation ..." << std::endl;
        return 0;
    }

    // Checkar att sourcepath faktiskt finns
    if (!checkFileName(getWorkingDirectory(), sourcepath))
    {
        std::cout << "ERROR : You cannot copy contents of a file that does not exist. Canceling copy operation ..." << std::endl;
        return 0;
    }
    // Checkar att destpath faktiskt inte finns
    if (checkFileName(getWorkingDirectory(), destpath))
    {

        dir_entry destPath = getFileDirEntry(destpath);
        // Kontrollera om destpath är fil eller dir
        if (destPath.type == TYPE_FILE)
        {
            // Om det är en existerande fil gör vi inget o returnerar en error
            std::cout << "ERROR : You cannot create a file with the same name as another. Canceling copy operation ..." << std::endl;
            return 0;
        }
        else
        {
            // Vi vet att de är en dir
            dir_entry fileToCopy = getFileDirEntry(sourcepath);
            std::string fileData = getFileData(fileToCopy);
            if (!checkFileName(destPath.first_blk, fileToCopy.file_name))
            {
                if (checkCwdSpace())
                {
                    create(sourcepath, fileData, destPath.first_blk);
                }
                else
                {
                    std::cout << "ERROR : No place in that directory. Canceling copy operation ..." << std::endl;
                }
            }
            else
            {
                std::cout << "ERROR : A file with that name does already exist. Canceling copy operation ..." << std::endl;
            }
            return 0;
        }
    }

    if (!checkCwdSpace())
    {
        std::cout << "ERROR : No place in current directory. Canceling copy operation ..." << std::endl;
        return 0;
    }

    // Läs datan från filen alltså sourcepath
    // Hämtar data om nuvarande directory
    dir_entry fileToCopy = getFileDirEntry(sourcepath);
    std::string fileData = getFileData(fileToCopy);

    create(destpath, fileData, getWorkingDirectory());

    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    // Error checks

    if (destpath.size() >= 56)
    {
        std::cout << "ERROR : New filename to large. Canceling rename operation ..." << std::endl;
        return 0;
    }

    // Checkar att sourcepath faktiskt finns
    if (!checkFileName(getWorkingDirectory(), sourcepath))
    {
        std::cout << "ERROR : You cannot rename a file that does not exist. Canceling rename operation ..." << std::endl;
        return 0;
    }

    // Checkar att destpath
    dir_entry destPath = getFileDirEntry(destpath);

    if (checkFileName(getWorkingDirectory(), destpath) && destPath.type != TYPE_DIR)
    {
        // Kontrollera om destpath existerar och att de är en fil

        std::cout << "ERROR : You cannot rename a file with the same name as another. Canceling rename operation ..." << std::endl;
        return 0;
    }

    // Kan vara dir, kan vara file, vet ej än
    int currentCwd = getWorkingDirectory();

    if (destPath.type == TYPE_DIR)
    {
        dir_entry currentFile = getFileDirEntry(sourcepath);
        std::string fileData = getFileData(currentFile);
        // Sätt temp working dir
        setTempCwd(destPath.first_blk);
        int tempCwd = getTempCwd();

        if (!checkFileName(tempCwd, currentFile.file_name))
        {

            // Critical section, funktionerna bygger på cwd
            if (checkCwdSpace())
            {
                updateDiskDirEntry(currentFile, DELETE, currentCwd);
                updateDiskDirEntry(currentFile, CREATE, tempCwd);
            }
            else
            {
                std::cout << "ERROR : That sub-dir is full. Canceling move operation ..." << std::endl;
            }
        }
        else
        {
            std::cout << "ERROR : A file with that name does already exist in that directory. Canceling move operation ..." << std::endl;
        }
    }
    else
    {
        // Renamar fillen bara
        dir_entry currentFile = getFileDirEntry(sourcepath);
        strcpy(currentFile.file_name, destpath.c_str());
        // NU ska vi uppdater dir_entryn i disken.
        updateDiskDirEntry(currentFile, UPDATE, currentCwd);
    }

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";

    // Först error checks
    if (!checkFileName(getWorkingDirectory(), filepath))
    {
        std::cout << "ERROR : A file with that name does not exist. Canceling file deleting ..." << std::endl;
        return 0;
    }

    // Vi har alltså fått en fil som ska bortas, vad ska ändras?
    // FAT måste uppdateras och Nuvarande dir måste uppdateras
    int cwd = getWorkingDirectory();

    dir_entry fileToDelete = getFileDirEntry(filepath);
    if (fileToDelete.type == TYPE_FILE)
    {
        updateFat(fileToDelete, DELETE);
        updateDiskDirEntry(fileToDelete, DELETE, cwd);
    }
    else
    {
        // Kontrollera om directoryn är tom
        if (checkEmptyDirectory(fileToDelete))
        {
            updateFat(fileToDelete, DELETE);
            updateDiskDirEntry(fileToDelete, DELETE, cwd);
        }
        else
        {
            std::cout << "ERROR : Directory is not empty. Canceling file deleting ..." << std::endl;
        }
    }

    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    // Error checks

    // Checkar att filepath1 faktiskt finns
    if (!checkFileName(getWorkingDirectory(), filepath1))
    {
        std::cout << "ERROR : You cannot append from a file that does not exist. Canceling append operation ..." << std::endl;
        return 0;
    }
    // Checkar att filepath2 faktiskt inte finns
    if (!checkFileName(getWorkingDirectory(), filepath2))
    {
        std::cout << "ERROR : You cannot append from a file to a file that does not exist. Canceling append operation ..." << std::endl;
        return 0;
    }

    // Get file 1 dir contents
    dir_entry file1 = getFileDirEntry(filepath1);
    // Then file 1 contents
    std::string file1Data = getFileData(file1);
    // Get file 2 dir contents
    dir_entry file2 = getFileDirEntry(filepath2);
    // Then file 2 contents
    std::string file2Data = getFileData(file2);
    // Complete data string
    std::string completFileData = file2Data + file1Data;

    // Now we shall store the new data to file2 and change fat and diskDirEntry.
    // Smart lösning är att ta bort file2 temporärt för att sedan skappa den på nytt med nya contentes
    rm(file2.file_name);
    create(file2.file_name, completFileData, getWorkingDirectory());

    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";

    if (dirpath.size() >= 56)
    {
        std::cout << "ERROR : Filename to large. Canceling directory creation ..." << std::endl;
        return 0;
    }
    if (checkFileName(getWorkingDirectory(), dirpath))
    {
        std::cout << "ERROR : A file/dir with that name does already exist. Canceling directory creation ..." << std::endl;
        return 0;
    }

    if (!checkCwdSpace())
    {
        std::cout << "ERROR : No place in current directory. Canceling directory creation ..." << std::endl;
        return 0;
    }

    // Skapar en Directory entry för filen.
    dir_entry newDirEntry = initDirEntry(dirpath, 0, TYPE_DIR);
    // Skapar faktiskta innehåller newDirEntry har
    dir_entry newSubDirectory[BLOCK_SIZE / 64];
    // Sätter subdirens första dir entry som en parent
    dir_entry parentEntry = initDirEntry("..", 0, TYPE_DIR);
    // detta gäller för parrententry enbart
    parentEntry.first_blk = getWorkingDirectory();
    newSubDirectory[0] = parentEntry;
    disk.write(newDirEntry.first_blk, (uint8_t *)&newSubDirectory);

    // updateFat, uppdaterar enbart fat, om vart filen ska vara osv.
    updateFat(newDirEntry, CREATE);
    updateDiskDirEntry(newDirEntry, CREATE, getWorkingDirectory());

    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";

    // Get dirPath contents
    std::vector<std::string> pathname = getAbsoluteFilepath(dirpath);

    // check if dirpath actually exist
    int subDirCwd = testAbsoluteFilepath(pathname);
    if (subDirCwd != -1)
    {
        setWorkingDirectory(subDirCwd);
    }

    if (!checkFileName(getWorkingDirectory(), dirpath) && dirpath != "..")
    {
        std::cout << "ERROR : A directory with that name does not exist. Canceling directory navigation ..." << std::endl;
        return 0;
    }

    dir_entry getDir = getFileDirEntry(dirpath);

    // check om det faktskt är en directory
    if (getDir.type == TYPE_FILE)
    {
        std::cout << "ERROR : You cannot change working directory to a file. Canceling directory navigation ..." << std::endl;
        return 0;
    }
    if (dirpath == "..")
    {
        dir_entry currentCWD[BLOCK_SIZE / sizeof(dir_entry)];
        getCurrentWorkDirectoryEntries(currentCWD, getWorkingDirectory());
        setWorkingDirectory(currentCWD[0].first_blk);
    }
    else
    {
        std::cout << getDir.first_blk;
        setWorkingDirectory(getDir.first_blk);
    }

    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd()
{
    std::cout << "FS::pwd()\n";

    std::string output = getPwd();

    std::cout << output << std::endl;

    /*for (int i = 0; i < 20; i++)
    {
        std::cout << "index " << i << " : " << fat[i] << std::endl;
    }*/

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
    currentDir.access_rights = READ | WRITE;

    return currentDir;
}

// This function does changes in FAT when a new file takes place, it also returns the first block of file
// Kanske ändra i fatten, samt i disken????

int FS::getFirstFreeFatBlock(int16_t fatTable[BLOCK_SIZE / 2])
{
    // Sätter minus ett, om de inte hittar vet vi om det.
    //  Det betyder att faten är full. Om de inte går ska de givetvis hanteras.
    int first_empty_index = -1;

    for (int i = 2; i < BLOCK_SIZE / 2; i++)
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

    for (int i = firstBlock + 1; i < BLOCK_SIZE / 2; i++)
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

std::string FS::getFileData(dir_entry file)
{
    std::string stringData = "";

    if (file.size < BLOCK_SIZE)
    {
        char data[BLOCK_SIZE];
        disk.read(file.first_blk, (uint8_t *)data);
        stringData = data;
    }
    else
    {
        // ger oss en array på alla index som fat, sedan adderar vi i stringData
        int *dataBlocks;
        dataBlocks = getFileBlockLocation(file.first_blk);

        for (int i = 0; i < BLOCK_SIZE / 2; i++)
        {
            if (dataBlocks[i] == -1)
            {
                break;
            }
            char data[BLOCK_SIZE];
            disk.read(dataBlocks[i], (uint8_t *)data);
            stringData += data;
        }
        delete[] dataBlocks;
    }
    return stringData;
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
    dir_entry currentCwd[BLOCK_SIZE / 64];
    getCurrentWorkDirectoryEntries(currentCwd, getWorkingDirectory());

    for (int i = 0; i < BLOCK_SIZE / 64; i++)
    {
        // då vet vi att vi hittat
        if (currentCwd[i].file_name == fileName)
        {
            file = currentCwd[i];
            break;
        }
    }
    return file;
}

void FS::getCurrentWorkDirectoryEntries(dir_entry *currentWorkDir, int workdirectory)
{
    dir_entry dirData[BLOCK_SIZE / 64];
    disk.read(workdirectory, (uint8_t *)dirData);
    memcpy(currentWorkDir, dirData, sizeof(dirData));
}

std::vector<std::string> FS::getAbsoluteFilepath(std::string filepath)
{
    // Relative filepath, add current working directory
    char first_char = filepath[0];
    if (first_char != '/')
        filepath = getPwd() + "/" + filepath;

    std::vector<std::string> tmp_filepath_content;
    std::vector<std::string> filepath_content;
    std::string tmp_filepath;
    std::stringstream ss(filepath);
    // separate filepath by '/', skip '.' in vector: tmp_filepath_content
    while (!ss.eof())
    {
        std::getline(ss, tmp_filepath, '/');
        if (!tmp_filepath.empty() && tmp_filepath != ".")
            tmp_filepath_content.push_back(tmp_filepath);
    }
    for (int i = 0; i < tmp_filepath_content.size(); i++)
    {
        if (tmp_filepath_content[i] == "..")
        {
            for (int j = i - 1; j >= 0; j--)
            {
                if (tmp_filepath_content[j] != "..")
                {
                    tmp_filepath_content[j] = "..";
                    break;
                }
            }
        }
    }
    for (int i = 0; i < tmp_filepath_content.size(); i++)
    {
        if (tmp_filepath_content[i] != "..")
            filepath_content.push_back(tmp_filepath_content[i]);
    }

    return filepath_content;
}

int FS::testAbsoluteFilepath(std::vector<std::string> filepathcontents)
{
    // Bara cd tänkt
    // Loopa igenom vectorn för att testa alla subdirs.(om de finns)
    int currentCwd = ROOT_BLOCK;

    for (int i = 0; i < filepathcontents.size(); i++)
    {
        dir_entry cwd[BLOCK_SIZE / sizeof(dir_entry)];
        getCurrentWorkDirectoryEntries(cwd, currentCwd);
        if (!checkFileName(currentCwd, filepathcontents[i]))
        {
            return -1;
        }
        dir_entry currentDir = getFileDirEntry(filepathcontents[i]);
        if (currentDir.type == TYPE_DIR)
        {
            currentCwd = currentDir.first_blk;
        }
    }
    return currentCwd;
}

// Returnerar sant om ett filnamn finns,
bool FS::checkFileName(int currentWorkDir, std::string filename)
{
    bool nameExist = false;

    // Läs från disk o få data.
    dir_entry currentCwd[BLOCK_SIZE / 64];
    getCurrentWorkDirectoryEntries(currentCwd, currentWorkDir);

    for (int i = 0; i < BLOCK_SIZE / 64; i++)
    {
        if (currentCwd[i].first_blk != 65535 && currentCwd[i].file_name == filename)
        {
            nameExist = true;
            break;
        }
    }

    return nameExist;
}

std::string FS::getPwd()
{
    std::string pwd = "";
    int cwd = getWorkingDirectory();
    if (cwd != ROOT_BLOCK)
    {
        dir_entry currentCwd[BLOCK_SIZE / 64];
        getCurrentWorkDirectoryEntries(currentCwd, cwd);

        // Vi använder oss av vekotr då det är smidigt när vi inte vet hur många subdirs vi har att göra med
        std::vector<std::string> allSubDirs;

        int currentBlock = cwd;
        int lastBlock = currentCwd[0].first_blk;

        while (currentBlock != ROOT_BLOCK)
        {
            dir_entry lastDirectory[BLOCK_SIZE / sizeof(dir_entry)];
            // Funkitonen nedans namn är lite missvisande, det är faktiskt inte nuvarnde cwd som vi hämtar data från.
            getCurrentWorkDirectoryEntries(lastDirectory, lastBlock);

            for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
            {
                if (lastDirectory[i].first_blk == currentBlock)
                {
                    allSubDirs.push_back(lastDirectory[i].file_name);
                    break;
                }
            }
            currentBlock = lastBlock;
            lastBlock = lastDirectory[0].first_blk;
        }

        // Vi läser från vektorn, baklänges då pathen blir skriven på de visset
        // detta då vi lagt till namn allt eftersom tills vi är i root.
        // Loop baklänges
        for (int i = allSubDirs.size() - 1; i >= 0; i--)
        {
            pwd += "/" + allSubDirs[i];
        }
    }
    else
    {
        // Root cwd, returnerar / enkelt.
        pwd = "/";
    }
    return pwd;
}

bool FS::checkCwdSpace()
{
    bool cwdSpace = false;

    dir_entry currentCwd[BLOCK_SIZE / 64];
    getCurrentWorkDirectoryEntries(currentCwd, getWorkingDirectory());

    for (int i = 1; i < BLOCK_SIZE / 64; i++)
    {
        // 65535 är högsta value i unit16_t. Alla first.blk har blivit de som default
        //  Är firsdt_blk === 65535 vet vi att vi kan använda det då inget block i fat har index 65535.
        // Om ej vet vi att ingen plats existerar;
        if (currentCwd[i].first_blk == 65535)
        {
            cwdSpace = true;
            break;
        }
    }
    return cwdSpace;
}

bool FS::checkEmptyDirectory(dir_entry dirname)
{
    dir_entry currentCwd[BLOCK_SIZE / 64];
    getCurrentWorkDirectoryEntries(currentCwd, dirname.first_blk);
    bool empty = true;

    // Loop igenom hela currentCWD för att se om den är tom eller ej
    for (int i = 1; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (currentCwd[i].first_blk != 65535)
        {
            // ingen file kan ha block 65535, om någon inte har de vet vi att de är en fil alternativt directory
            empty = false;
            break;
        }
    }

    return empty;
}

void FS::updateDiskDirEntry(dir_entry newFile, int deleteOrCreate, int cwd)
{
    dir_entry currentCwd[BLOCK_SIZE / 64];
    getCurrentWorkDirectoryEntries(currentCwd, cwd);

    if (deleteOrCreate == CREATE)
    {
        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            // 65535 är högsta value i unit16_t. Alla first.blk har blivit de som default
            //  Är firsdt_blk === 65535 vet vi att vi kan använda det då inget block i fat har index 65535.
            if (currentCwd[i].first_blk == 65535)
            {
                currentCwd[i] = newFile;
                break;
            }
        }
    }
    else
    {
        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            if (currentCwd[i].first_blk == newFile.first_blk)
            {
                // Då hittade vi filen som ska bortas
                if (deleteOrCreate == DELETE)
                {
                    currentCwd[i].first_blk = 65535;
                }
                else
                {
                    // Update scenario
                    currentCwd[i] = newFile;
                }

                break;
            }
        }
    }

    disk.write(cwd, (uint8_t *)&currentCwd);
}

void FS::updateFat(dir_entry currentFile, int deleteOrCreate)
{
    // Update fat and Disk
    int amountOfBlocks = getAmountOfBlocks(currentFile.size);
    // VI går in i denna if sats om  deleteOrCreate == CREATE då de är 1.
    if (deleteOrCreate)
    {
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
    else
    {
        // uppdatera fat på ett sådant sätt som tar bort filen från fat
        int *fatLocation = getFileBlockLocation(currentFile.first_blk);
        for (int i = 0; i < amountOfBlocks; i++)
        {
            fat[fatLocation[i]] = FAT_FREE;
        }
        delete[] fatLocation;
        disk.write(FAT_BLOCK, (uint8_t *)&fat);
    }
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
