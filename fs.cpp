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

    std::vector<std::string> pathname = getAbsoluteFilepath(filepath);
    if (pathname.empty() || filepath == "./")
    {
        std::cout << "ERROR : Not a valid name" << std::endl;
        return 0;
    }
    std::string filename = pathname.back();

    std::regex regex_filename("^[\\w\\-. ]+$");
    if (!std::regex_match(filename, regex_filename))
    {
        std::cout << "ERROR : Filename must only contain [a-z] [A-Z] [0-9] . _ -" << std::endl;
        return 0;
    }
    // max filename size: 55 characters
    if (filename.size() >= 56)
    {
        std::cout << "ERROR : Filename must not exceed 55 characters" << std::endl;
        return 0;
    }

    // check if dirpath actually exist
    int subDirCwd = testAbsoluteFilepath(pathname, TYPE_FILE);
    if (subDirCwd != -1)
    {
        std::cout << "ERROR" << std::endl;
        return 0;
    }

    pathname.pop_back();
    subDirCwd = testAbsoluteFilepath(pathname, TYPE_DIR);
    if (subDirCwd == -1)
    {
        std::cout << "ERROR" << std::endl;
        return 0;
    }

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
    dir_entry newFile = initDirEntry(filename, completeInput.size(), TYPE_FILE);

    // updateFat, uppdaterar enbart fat, om vart filen ska vara osv.
    diskWrite(newFile, completeInput);
    updateFat(newFile, CREATE);
    updateDiskDirEntry(newFile, CREATE, subDirCwd);

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

    std::vector<std::string> pathname = getAbsoluteFilepath(filepath);
    // check if dirpath actually exist
    int subDirCwd = testAbsoluteFilepath(pathname, TYPE_FILE);

    if (subDirCwd != -1)
    {
        // DÅ ska vi läsa filen!
        dir_entry fileToRead = getFileDirEntry(pathname.back(), subDirCwd);
        std::string output = getFileData(fileToRead);
        std::cout << output << std::endl;
    }
    else
    {
        std::cout << "ERROR" << std::endl;
    }

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

    std::vector<std::string> pathname1 = getAbsoluteFilepath(sourcepath);
    std::vector<std::string> pathname2 = getAbsoluteFilepath(destpath);
    std::string filename = pathname1.back();
    std::string temp2 = pathname2.back();

    // sourcepath
    int subdircwd1 = testAbsoluteFilepath(pathname1, TYPE_FILE);
    // destpath
    int subdircwd2 = testAbsoluteFilepath(pathname2, TYPE_DIR);

    int subdircwd3 = testAbsoluteFilepath(pathname2, TYPE_FILE);

    // path 1 MÅSTEE! finnas då det är den fil vi ska behandla
    if (subdircwd1 == -1)
    {
        std::cout << "ERROR" << std::endl;
        return 0;
    }

    if (subdircwd3 != -1)
    {
        // FILE EXIST, du kan inte move tille en fil (or rename)
        std::cout << "ERROR : You cannot cp to a filename that already exist" << std::endl;
        return 0;
    }

    if (subdircwd2 != -1)
    {
        // Då existerar directoryn, kontrollera nu bara att ingen fil vid samma namn finns där
        if (checkFileName(subdircwd2, filename))
        {
            std::cout << "ERROR : FILE EXIST" << std::endl;
        }
        else
        {
            dir_entry FiletoCopy = getFileDirEntry(filename, subdircwd1);
            std::string fileToCopyData = getFileData(FiletoCopy);
            create(filename, fileToCopyData, subdircwd2);
        }
    }
    else
    {
        // KUl scenario subdircwd2 finns inte, alltså pathen existera inte men.
        // Det kan vara så att vi vill rename vår nuvarande fil.
        // Kan vara så att pathname2 är tom, alltså att vi är i root, då måste vi göra något
        std::vector<std::string> rename = pathname2;
        rename.pop_back();
        int checkIfRename = testAbsoluteFilepath(rename, TYPE_DIR);
        if (checkIfRename == subdircwd1)
        {
            dir_entry fileToCopy = getFileDirEntry(filename, subdircwd1);
            std::string fileToCopyData = getFileData(fileToCopy);
            create(temp2, fileToCopyData, subdircwd1);
        }
        else
        {
            std::cout << "ERROR : Not a valid path" << std::endl;
        }
    }
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    std::vector<std::string> pathname1 = getAbsoluteFilepath(sourcepath);
    std::vector<std::string> pathname2 = getAbsoluteFilepath(destpath);
    std::string filename = pathname1.back();

    // sourcepath
    int subdircwd1 = testAbsoluteFilepath(pathname1, TYPE_FILE);
    // destpath
    int subdircwd2 = testAbsoluteFilepath(pathname2, TYPE_DIR);

    int subdircwd3 = testAbsoluteFilepath(pathname2, TYPE_FILE);

    // path 1 MÅSTEE! finnas då det är den fil vi ska behandla
    if (subdircwd1 == -1)
    {
        std::cout << "ERROR" << std::endl;
        return 0;
    }

    if (subdircwd3 != -1)
    {
        // FILE EXIST, du kan inte move tille en fil (or rename)
        std::cout << "ERROR : You cannot move to a existing file" << std::endl;
        return 0;
    }
    if (subdircwd2 != -1)
    {
        // Då existerar directoryn, kontrollera nu bara att ingen fil vid samma namn finns där
        if (checkFileName(subdircwd2, filename))
        {
            std::cout << "ERROR : FILE EXIST" << std::endl;
        }
        else
        {
            dir_entry currentFile = getFileDirEntry(filename, subdircwd1);

            updateDiskDirEntry(currentFile, DELETE, subdircwd1);
            updateDiskDirEntry(currentFile, CREATE, subdircwd2);
        }
        return 0;
    }
    else
    {
        // KUl scenario subdircwd2 finns inte, alltså pathen existera inte men.
        // Det kan vara så att vi vill rename vår nuvarande fil.
        // Kan vara så att pathname2 är tom, alltså att vi är i root, då måste vi göra något
        std::vector<std::string> rename = pathname2;
        rename.pop_back();
        int checkIfRename = testAbsoluteFilepath(rename, TYPE_DIR);
        if (checkIfRename == subdircwd1)
        {
            std::string newFileName = pathname2.back();
            dir_entry currentFile = getFileDirEntry(filename, subdircwd1);
            strcpy(currentFile.file_name, newFileName.c_str());
            // NU ska vi uppdater dir_entryn i disken.
            updateDiskDirEntry(currentFile, UPDATE, subdircwd1);
        }
        else
        {
            std::cout << "ERROR : Not a valid path" << std::endl;
        }
    }
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";

    std::vector<std::string> pathname = getAbsoluteFilepath(filepath);
    // check if dirpath actually exist
    int subDirCwd = testAbsoluteFilepath(pathname, TYPE_FILE);

    if (subDirCwd != -1)
    {
        // DÅ ska vi remova filen filen!
        dir_entry fileToDelete = getFileDirEntry(pathname.back(), subDirCwd);
        updateFat(fileToDelete, DELETE);
        updateDiskDirEntry(fileToDelete, DELETE, subDirCwd);
        return 0;
    }

    subDirCwd = testAbsoluteFilepath(pathname, TYPE_DIR);
    if (subDirCwd != -1)
    {

        dir_entry cwd[BLOCK_SIZE / sizeof(dir_entry)];
        getCurrentWorkDirectoryEntries(cwd, subDirCwd);
        int parentcwd = cwd[0].first_blk;

        dir_entry fileToDelete = getFileDirEntry(pathname.back(), parentcwd);

        if (checkEmptyDirectory(fileToDelete))
        {
            updateFat(fileToDelete, DELETE);
            updateDiskDirEntry(fileToDelete, DELETE, parentcwd);
        }
        else
        {
            std::cout << "ERROR : Directory is not empty. Canceling file deleting ..." << std::endl;
        }
    }
    else
    {
        std::cout << "ERROR : File or Dir does not exist. " << std::endl;
    }

    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";

    std::vector<std::string> pathname1 = getAbsoluteFilepath(filepath1);
    std::vector<std::string> pathname2 = getAbsoluteFilepath(filepath2);
    int subdircwd1 = testAbsoluteFilepath(pathname1, TYPE_FILE);
    int subdircwd2 = testAbsoluteFilepath(pathname2, TYPE_FILE);

    if (subdircwd1 == -1)
    {
        std::cout << "ERROR" << std::endl;
        return 0;
    }
    if (subdircwd2 == -1)
    {
        std::cout << "ERROR" << std::endl;
        return 0;
    }

    // check if dirpath actually exist

    // Get file 1 dir contents
    dir_entry file1 = getFileDirEntry(pathname1.back(), subdircwd1);
    // Then file 1 contents
    std::string file1Data = getFileData(file1);
    // Get file 2 dir contents
    dir_entry file2 = getFileDirEntry(pathname2.back(), subdircwd2);
    // Then file 2 contents
    std::string file2Data = getFileData(file2);
    // Complete data string
    std::string completFileData = file2Data + file1Data;

    // Now we shall store the new data to file2 and change fat and diskDirEntry.
    // Smart lösning är att ta bort file2 temporärt för att sedan skappa den på nytt med nya contentes
    rm(filepath2);
    create(file2.file_name, completFileData, subdircwd2);

    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";

    std::vector<std::string> pathname = getAbsoluteFilepath(dirpath);

    // HMmmmmm....
    if (pathname.empty() || dirpath == "./")
    {
        std::cout << "ERROR : Not a valid name" << std::endl;
        return 0;
    }
    std::string dirname = pathname.back();

    int subDirCwd = testAbsoluteFilepath(pathname, TYPE_DIR);
    int subDir3 = testAbsoluteFilepath(pathname, TYPE_FILE);
    pathname.pop_back();
    int subdir2 = testAbsoluteFilepath(pathname, TYPE_DIR);

    if (subDirCwd != -1 || subdir2 == -1)
    {
        std::cout << "ERROR" << std::endl;
        return 0;
    }
    if (subDir3 != -1)
    {
        std::cout << "ERROR" << std::endl;
        return 0;
    }

    // Nu kan vi skappa diren

    // Validate filename
    // regex: [a-z] [A-Z] [0-9] . _ -
    std::regex regex_dirname("^[\\w\\-. ]+$");
    if (!std::regex_match(dirname, regex_dirname))
    {
        std::cout << "ERROR : Directory name must only contain [a-z] [A-Z] [0-9] . _ -" << std::endl;
        return 0;
    }
    // max filename size: 55 characters
    if (dirname.size() >= 56)
    {
        std::cout << "ERROR : Directory name must not exceed 55 characters" << std::endl;
        return 0;
    }

    // Skapar en Directory entry för filen.
    dir_entry newDirEntry = initDirEntry(dirname, 0, TYPE_DIR);
    // Skapar faktiskta innehåller newDirEntry har
    dir_entry newSubDirectory[BLOCK_SIZE / 64];
    // Sätter subdirens första dir entry som en parent
    dir_entry parentEntry = initDirEntry("..", 0, TYPE_DIR);
    // detta gäller för parrententry enbart
    parentEntry.first_blk = subdir2;
    newSubDirectory[0] = parentEntry;
    disk.write(newDirEntry.first_blk, (uint8_t *)&newSubDirectory);

    // updateFat, uppdaterar enbart fat, om vart filen ska vara osv.
    updateFat(newDirEntry, CREATE);
    // Uppdaterar diskDirEnry
    updateDiskDirEntry(newDirEntry, CREATE, subdir2);

    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";

    // Get dirPath contents
    std::vector<std::string> pathname = getAbsoluteFilepath(dirpath);
    // check if dirpath actually exist
    int subDirCwd = testAbsoluteFilepath(pathname, TYPE_DIR);

    if (subDirCwd != -1)
    {
        setWorkingDirectory(subDirCwd);
    }
    else
    {
        std::cout << "ERROR" << std::endl;
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

dir_entry FS::getFileDirEntry(std::string fileName, int cwd)
{
    dir_entry file;
    dir_entry currentCwd[BLOCK_SIZE / 64];
    getCurrentWorkDirectoryEntries(currentCwd, cwd);

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

int FS::testAbsoluteFilepath(std::vector<std::string> filepathcontents, bool type)
{
    // Bara cd tänkt
    // Loopa igenom vectorn för att testa alla subdirs.(om de finns)
    // nedan är fel
    int currentCwd = ROOT_BLOCK;
    int counter = filepathcontents.size();
    if (type == TYPE_FILE)
        counter--;
    bool nameExist;

    for (int i = 0; i < counter; i++)
    {
        dir_entry cwd[BLOCK_SIZE / sizeof(dir_entry)];
        getCurrentWorkDirectoryEntries(cwd, currentCwd);

        nameExist = false;
        for (int j = 0; j < BLOCK_SIZE / 64; j++)
        {
            if (cwd[j].file_name == filepathcontents[i] && cwd[j].type == TYPE_DIR)
            {
                currentCwd = cwd[j].first_blk;
                nameExist = true;
                break;
            }
        }
        if (!nameExist)
            return -1;
    }

    if (type == TYPE_FILE)
    {
        dir_entry testFile = getFileDirEntry(filepathcontents.back(), currentCwd);
        if (testFile.first_blk != 65535 && testFile.type == TYPE_FILE)
        {
            nameExist = true;
        }
        else
        {
            nameExist = false;
        }
        if (!nameExist)
            return -1;
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
