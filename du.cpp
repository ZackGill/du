#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <stdint.h>
#include <math.h>
#include <tuple>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

/*
 *	Usage: du [options] [pathname]
 *	my_du does not assume -b (add directory size), needs that option flag.
 *	my_du defaults to printing out bytes.
 *	if no pathname given, use current directory.
 *	Implemented options: -b, -h, -S, -g: all as du except -S is for sort by size, -g is for a-level graph data
 *	Data gathered for -g: when files (sorted by type) are modified (do I code at night? When do I start?)
 *	Also determine average file size.
 */


/*	recurseDir returns the size of the directory 
 *	It uses lstat for any stat syscalls to give size of links instead of size of files links link to.
 */

// Data struct used for A-Level things, with regards to directories.
// Also used for number of files in directory.
// Using a struct so it is easier to change later if I want different types of data.
typedef struct{
	unsigned long int numFiles;
	unsigned long int bytes;
}data;

// Vector of graph data: tuples of filename, date modified, size of file (bytes).
vector<tuple<string, time_t, unsigned long int>> graphData;



// Vector of directory name, size, and struct data.
vector<tuple<string, unsigned long int, data>> directories;


// Compartor to sort dir on size.
bool sizeCompare(tuple<string, unsigned long int, data> first, tuple<string, unsigned long int, data> second)
{
	return get<1>(first) < get<1>(second);
}
// Compartor to sort dir by name.
bool nameCompare(tuple<string, unsigned long int, data> first, tuple<string, unsigned long int, data> second)
{
	return (get<0>(first).compare(get<0>(second)) < 0);
}

/*	PrintDir prints directories. Like du, replaces current directory with .
 *	sort = sort by size, graph = output data for graph, currentDir = working with currentDir, human = human readable
*/
void printDir(bool sortS, bool graph, bool human, bool currentDir)
{
	if(directories.size() < 1){
		printf("nothing to print\n");
		return;
	}
	if(sortS){
		sort(directories.begin(), directories.end(), sizeCompare);
	}
	else{
		sort(directories.begin(), directories.end(), nameCompare);
	}

	char *path = (char *)(malloc(PATH_MAX));
	getcwd(path, PATH_MAX);
	if(!currentDir)
		free(path);
	string CPath = path;

	// print everything in directories. Replace current directory with . if currentDir is set.
	printf("My du output is: num files, size, directory\n");
	for(auto a: directories){
		if(currentDir){
			size_t found = get<0>(a).find(CPath);
			if(found != string::npos){
				string repl = ".";
				get<0>(a).replace(found, CPath.length(), repl);
			}
		}
		unsigned long int printSize = get<1>(a);
		if(human)
			printSize = ceil(printSize/1024.0);
		if(human)
			printf("%lu \t %lu K \t %s\n", get<2>(a).numFiles, printSize, get<0>(a).c_str());
		else
			printf("%lu \t %lu \t %s\n", get<2>(a).numFiles, printSize, get<0>(a).c_str());
	}
	if(currentDir)
		free(path);


	if(graph){
		FILE *file = fopen("graphData", "w");

		string datestring;
		struct tm *tm;
		unsigned long int Kbytes, num_files;
		num_files = 0;
		Kbytes = 0;
		for(auto a: graphData){
			tm = localtime(&get<1>(a));

			if(tm->tm_hour >= 18)
				datestring = "evening";
			else if(tm->tm_hour >= 12)
				datestring = "afternoon";
			else if(tm->tm_hour >= 6)
				datestring = "morning";
			else
				datestring = "all-nighter";

			fprintf(file, "%s, %s\n", get<0>(a).c_str(), datestring.c_str());
			Kbytes += ceil(get<2>(a)/1024.0);
			num_files++;
		}
		fprintf(file, "average, %lu\n", Kbytes/num_files);
		fclose(file);
	}

}


unsigned long int recurseDir(char *dirPath, bool totalSize, bool human)
{
	DIR *dirPtr;
	struct dirent *entryPtr;
	dirPtr = opendir(dirPath);
	if(dirPtr == NULL){
		perror("Error opening dir\n");
		return 0;
	}
	unsigned long int size = 0;
	unsigned long int num_files = 0;

	// If entryPtr is a directory, call self with that as new dirPath.
	// If not directory, call stat on file and add it to size.
	// if totalSize option is true, also call stat on directory (adding directory bytes to count).
	char *file = (char *)(malloc(PATH_MAX));
	while((entryPtr = readdir(dirPtr))){
		// skip self and parent, avoid infinite recursion (also du doesn't do that).
		// self is final return of recursive call anyway.
		if(strncmp(entryPtr->d_name, "..", 2) == 0){
			continue;
		}
		else if(strncmp(entryPtr->d_name, ".", 1) == 0){
			// If total size, then need to add self size to size.
			struct stat statBuf;
			if(totalSize){
				if(lstat(entryPtr->d_name, &statBuf) == -1){
					printf("%s\n", entryPtr->d_name);
					perror("Error ");
					continue;
				}
				if(human){
					auto tempBlock = ceil(statBuf.st_blocks / 2.0); // st_blocks in 512, half for 1024
					size += tempBlock * 1024;
				}
				else
					size += statBuf.st_size;
			}

			continue;
		}
		struct stat statBuf;
		sprintf(file, "%s/%s", dirPath, entryPtr->d_name);

		if(lstat(file, &statBuf) == -1){
			perror("error \n");
		}

		if(S_ISDIR (statBuf.st_mode)){
			unsigned long int tempSize = 0;
			tempSize += recurseDir(file, totalSize, human);
			if(human){
				auto tempBlock = ceil(tempSize/1024.0);
				tempSize = tempBlock * 1024;
			}
			size += tempSize;
		}
		else if(S_ISREG(statBuf.st_mode)){
			num_files++;
			if(lstat(file, &statBuf) == -1){
				printf("%s\n", file);
				perror("Error ");
				continue;
			}
			if(human){
				auto tempBlock = ceil(statBuf.st_blocks / 2.0); // st_blocks in 512, half for 1024
				size += tempBlock * 1024;
			}
			else
				size += statBuf.st_size;
			graphData.push_back(make_tuple(entryPtr->d_name, statBuf.st_mtime, statBuf.st_size));
		}
		else{
			printf("Type is not regular or directory\n");
		}

	}
	free(file);
	// using c++ string constructed from c string so sorting is "better".
	data temp;
	temp.numFiles = num_files;
	temp.bytes = 0; // Not implemented yet.
	directories.push_back(make_tuple(dirPath, size, temp));
	closedir(dirPtr);
	return size;

}


int main(int argc, char *argv[]){

	char *path = (char *)(malloc(PATH_MAX));

	char *options = (char *)(malloc(5));
	strcpy(options, "");
	bool currentDir = false;

	// if only 1 arg (program itself), use current working directory
	if(argc < 2){
		printf("using current working directory.\n");
		getcwd(path, PATH_MAX);
		currentDir = true;
	}

	// See if flag options or path name.
	if(argc == 2){
		
		// Flag only. Set option value and path to current dir.
		if(strncmp(argv[1], "-", 1) == 0){
			printf("Using current working directory with options.\n");
			strcpy(options, argv[1]);
			getcwd(path, PATH_MAX);
			currentDir = true;
		}
		else{ // Path only. Set path to arg.
			printf("Running du on path: %s\n", argv[1]);
			strcpy(path, argv[1]);
		}
	}

	// Flags and path
	if(argc > 2){
		if(strncmp(argv[1], "-", 1) != 0){
			printf("Error: perhaps you meant -options pathname?\n");
			free(path);
			free(options);
			return 0;
		}
		printf("Running du on path with options: %s\n", argv[2]);
		strcpy(options, argv[1]);
		strcpy(path, argv[2]);
	}

	bool human = false;
	if(strchr(options, 'h') != NULL)
		human = true;

	bool totalSize = false;
	if(strchr(options, 'b') != NULL)
		totalSize = true;
		
	recurseDir(path, totalSize, human);

	bool sortS, graph; // Sort means sort by size, graph means A-level output stuff.
	sortS = false;
	graph = false;
	
	if(strchr(options, 'S') != NULL)
		sortS = true;
	if(strchr(options, 'h') != NULL)
		human = true;
	if(strchr(options, 'g') != NULL)
		graph = true;

	printDir(sortS, graph, human, currentDir);


	free(path);
	free(options);
	return 0;
}
