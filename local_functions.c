#include "local_functions.h"

//checks whether main app has been called with valid parameters
//takes number of program arguments, list of those arguments
bool is_Call_Valid(int number_of_parameters, char *parameters[])
{
  if(number_of_parameters < 3)
  {
    syslog(LOG_ERR, "Not enought input arguments");
    return false;
  }
  if(is_Directory(parameters[1]))
  {
    if(is_Directory(parameters[2]))
      return true;
    syslog(LOG_ERR, "Second argument is not a directory");
    return false;
  }
  syslog(LOG_ERR, "First argument is not a directory");
  return false;
}

//checks whether given path points a directory
//takes path
bool is_Directory(char *path)
{
  struct stat s;
  if(stat(path, &s) == -1)
  {
    syslog(LOG_ERR, "Can't determine if %s is a directory; errno: %s", path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  if(s.st_mode & S_IFDIR)
    return true;
  return false;
}

//returns size of file
//takes path
off_t get_Size(char *path)
{
  struct stat size;
  if(stat(path, &size) == -1)
  {
    syslog(LOG_ERR, "Can't get size of file %s; errno: %s", path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  return size.st_size;
}

//returns modification date of a file
//takes path
time_t get_Time(char *path)
{
  struct stat time;
  if(stat(path, &time) == -1)
  {
    syslog(LOG_ERR, "Can't get modification date for %s; errno: %s", path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  return time.st_mtime;
}

//returns permissions of a file
//takes paths
mode_t get_Permissions(char *path)
{
  struct stat permissions;
  if(stat(path, &permissions) == -1)
  {
    syslog(LOG_ERR, "Can't get permissions for %s; errno: %s", path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  return permissions.st_mode;
}

//changes modification date of a file to match another file
//takes two paths
void change_Parameters(char *input, char *output)
{
  struct utimbuf time;
  time.actime = 0;
  time.modtime = get_Time(input);
  if(utime(output, &time) == -1)
  {
    syslog(LOG_ERR, "Can't modify modification date for file %s; errno: %s", output, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
}

//creates a valid path to a file from path to a directory and filename
//takes path to directory and filename
char *add_To_Path(char *path, char *file_name)
{
  int len = strlen(path) + strlen(file_name) + 1;
  char *new_path = (char *)malloc(len * sizeof(char));
  if(new_path == NULL)
  {
    syslog(LOG_ERR, "Malloc function misfunction; errno: %s", strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  strcpy(new_path, path);
  strcat(new_path,"/");
  strcat(new_path,file_name);
  return new_path;
}

//checks if there is a maching file or directory in given folder
//takes filename and path to a directory
bool are_Same(char *file_name, char *input_folder_path, char *output_folder_path)
{
  bool check = false;
  struct dirent *file;
  DIR *catalog_path;
  char *old_path = add_To_Path(input_folder_path, file_name);
  char *new_path = add_To_Path(output_folder_path, file_name);
  catalog_path = opendir(output_folder_path);
  if(catalog_path == NULL)
  {
    syslog(LOG_ERR, "Could not open folder %s; errno: %s", output_folder_path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  while(file = readdir(catalog_path))
  {
    if(errno != 0)
    {
      syslog(LOG_ERR, "errno: %s", strerror(errno));
      syslog(LOG_NOTICE, "Daemon shutting down");
      exit(EXIT_FAILURE);
    }
    if(strcmp(file->d_name, file_name) == 0)
    {
      if((file -> d_type) == DT_REG)
      {
        if(!(get_Time(old_path) - get_Time(new_path)))
        {
          if(get_Permissions(old_path) == get_Permissions(new_path))
            check = true;
        }
      }
      else
      {
        check = true;
      }
    }
  }
  free(old_path);
  free(new_path);
  if(closedir(catalog_path) == -1)
  {
    syslog(LOG_ERR, "Could not close folder %s; errno: %s", output_folder_path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  return check;
}

void check_Existing(char *input_folder_path, char *output_folder_path, bool recursive)
{
  char *old_path;
  char *new_path;
  DIR *catalog_path;
  struct dirent *file, *content;
  catalog_path = opendir(output_folder_path);
  if(catalog_path == NULL)
  {
    syslog(LOG_ERR, "Could not open folder %s; errno: %s", output_folder_path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  while(file = readdir(catalog_path))
  {
    if(errno != 0)
    {
      syslog(LOG_ERR, "errno: %s", strerror(errno));
      syslog(LOG_NOTICE, "Daemon shutting down");
      exit(EXIT_FAILURE);
    }
    char *file_name = file -> d_name;
    if(file -> d_type == DT_DIR && recursive)
    {
      if(!(strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0))
      {
        old_path = add_To_Path(input_folder_path, file_name);
        new_path = add_To_Path(output_folder_path, file_name);
        if(are_Same(file_name, output_folder_path, input_folder_path))
        {
          //check recursively
          check_Existing(old_path, new_path, recursive);
        }
        else
        {
          delete_Folder(new_path);
        }
        free(old_path);
        free(new_path);
      }
    }
    else if((file -> d_type) == DT_REG)
    {
      old_path = add_To_Path(input_folder_path, file_name);
      new_path = add_To_Path(output_folder_path, file_name);
      if(access(new_path, F_OK) == 0)
      {
        if(!(are_Same(file_name, output_folder_path, input_folder_path)))
        {
          if(remove(new_path) == -1)
          {
            syslog(LOG_ERR, "Error while removing %s; errno: %s", new_path, strerror(errno));
            syslog(LOG_NOTICE, "Daemon shutting down");
            exit(EXIT_FAILURE);
          }
          syslog(LOG_INFO, "File %s deleted.", new_path);
        }
      }
      else
      {
        syslog(LOG_ERR, "Unable to acces path %s; errno %s", new_path, strerror(errno));
        syslog(LOG_NOTICE, "Daemon shutting down");
        exit(EXIT_FAILURE);
      }
      free(old_path);
      free(new_path);
    }
  }
  if(closedir(catalog_path) == -1)
  {
    syslog(LOG_ERR, "Could not close folder %s; errno: %s", output_folder_path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
}

void delete_Folder(char *path)
{
  char *removed_name;
  char *file_name;
  DIR *catalog_path;
  struct dirent *file;
  catalog_path = opendir(path);
  if(catalog_path == NULL)
  {
    syslog(LOG_ERR, "Could not open folder %s; errno: %s", path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  while(file = readdir(catalog_path))
  {
    if(errno != 0)
    {
      syslog(LOG_ERR, "errno: %s", strerror(errno));
      syslog(LOG_NOTICE, "Daemon shutting down");
      exit(EXIT_FAILURE);
    }
    file_name = file -> d_name;
    removed_name = add_To_Path(path, file_name);
    if(access(removed_name, F_OK) == 0)
    {
      if(file -> d_type == DT_DIR)
      {
        if(!(strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0))
        {
          delete_Folder(removed_name);
        }
      }
      else if(file -> d_type == DT_REG)
      {
        if(remove(removed_name) == -1)
        {
          syslog(LOG_ERR, "Error while removing %s; errno: %s", removed_name, strerror(errno));
          syslog(LOG_NOTICE, "Daemon shutting down");
          exit(EXIT_FAILURE);
        }
        syslog(LOG_INFO, "File %s deleted.", removed_name);
      }
    }
    else
    {
      syslog(LOG_ERR, "Unable to acces path %s; errno: %s", removed_name, strerror(errno));
      syslog(LOG_NOTICE, "Daemon shutting down");
      exit(EXIT_FAILURE);
    }
    free(removed_name);
  }
  if(closedir(catalog_path) == -1)
  {
    syslog(LOG_ERR, "Could not close folder %s; errno: %s", path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  if(remove(path) == -1)
  {
    syslog(LOG_ERR, "Error while removing %s; errno: %s", path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  syslog(LOG_INFO, "Catalog %s deleted.", path);
}

void copy_File(char *input, char *output)
{
  clock_t start, end;
  double cpu_time_used;
  start = clock();
  char buffer[/*131072*/1024];
  int input_file, output_file, read_input, read_output;
  input_file = open(input, O_RDONLY);
  if(input_file == -1)
  {
    syslog(LOG_ERR, "Can't open file %s; errno: %s", input, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  output_file = open(output, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if(output_file == -1)
  {
    syslog(LOG_ERR, "Can't open/create file %s; errno: %s", output, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  while((read_input = read(input_file, buffer, sizeof(buffer)))>0)
  {
    if(read_input == -1)
    {
      syslog(LOG_ERR, "Can't read from file %s; errno: %s", input, strerror(errno));
      syslog(LOG_NOTICE, "Daemon shutting down");
      exit(EXIT_FAILURE);
    }
    read_output = write(output_file, buffer, (ssize_t) read_input);
    if(read_output == -1)
    {
      syslog(LOG_ERR, "Can't write to file %s; errno: %s", output, strerror(errno));
      syslog(LOG_NOTICE, "Daemon shutting down");
      exit(EXIT_FAILURE);
    }
  }
  if(close(input_file) == -1)
  {
    syslog(LOG_ERR, "Can't close file %s; errno: %s", input, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  if(close(output_file) == -1)
  {
    syslog(LOG_ERR, "Can't close file %s; errno: %s", output, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  change_Parameters(input, output);
  if(chmod(output, get_Permissions(input)) == -1)
  {
    syslog(LOG_ERR, "Can't change permissions for %s; errno: %s", output, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  syslog(LOG_INFO, "Coppied in normal mode: File %s to %s.", input, output);
  end = clock();
  cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
  syslog(LOG_INFO, "KOPIOWANIE STANDARDOWE: %f", cpu_time_used);
}

void copy_File_By_Mapping(char *input, char *output)
{
  clock_t start, end;
  double cpu_time_used;
  start = clock();
  int size = get_Size(input);
  int input_file = open(input, O_RDONLY);
  if(input_file == -1)
  {
    syslog(LOG_ERR, "Can't open file %s; errno: %s", input, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  int output_file = open(output, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if(output_file == -1)
  {
    syslog(LOG_ERR, "Can't open/create file %s; errno: %s", output, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  char *map = (char*) mmap (0, size, PROT_READ, MAP_SHARED | MAP_FILE, input_file, 0);
  if(map == MAP_FAILED)
  {
    syslog(LOG_INFO,"Failed to map data from file %s; errno: %s", input, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  if(write(output_file, map, size) == -1)
  {
    syslog(LOG_ERR, "Can't write to file %s; errno: %s", output, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  if(close(input_file) == -1)
  {
    syslog(LOG_ERR, "Can't close file %s; errno: %s", input, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  if(close(output_file) == -1)
  {
    syslog(LOG_ERR, "Can't close file %s; errno: %s", output, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  if(munmap(map, size) == -1)
  {
    syslog(LOG_INFO,"Failed to unmap data; errno: %s", strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  change_Parameters(input, output);
  if(chmod(output, get_Permissions(input)) == -1)
  {
    syslog(LOG_ERR, "Can't change permissions for %s; errno: %s", output, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  syslog(LOG_INFO, "Coppied using mapping: File %s to %s.", input, output);
  end = clock();
  cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
  syslog(LOG_INFO, "KOPIOWANIE PRZEZ MAPOWANIE: %f", cpu_time_used);
}

void browse_Folder(char *input_folder_path, char *output_folder_path, bool recursive, int size_of_file)
{
  char *file_name;
  char *old_path;
  char *new_path;
  DIR *path, *tmp;
  struct dirent *file;
  path = opendir(input_folder_path);
  if(path == NULL)
  {
    syslog(LOG_ERR, "Could not open folder %s; errno: %s", input_folder_path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
  while(file = readdir(path))
  {
    if(errno != 0)
    {
      syslog(LOG_ERR, "errno: %s", strerror(errno));
      syslog(LOG_NOTICE, "Daemon shutting down");
      exit(EXIT_FAILURE);
    }
    file_name = file -> d_name;
    if((file -> d_type) == DT_REG)
    {
      old_path = add_To_Path(input_folder_path, file_name);
      new_path = add_To_Path(output_folder_path, file_name);
      if(!(are_Same(file_name, input_folder_path, output_folder_path)))

        if(get_Size(old_path)>size_of_file)
          copy_File_By_Mapping(old_path, new_path);
        else
          copy_File(old_path, new_path);
      free(old_path);
      free(new_path);
    }
    else if((file -> d_type) == DT_DIR && recursive)
    {
      if(!(strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0))
      {
        new_path = add_To_Path(output_folder_path, file_name);
        old_path = add_To_Path(input_folder_path, file_name);
        if(!(tmp = opendir(new_path)))
        {
          if(mkdir(new_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
          {
            syslog(LOG_ERR, "Could not create folder %s; errno: %s", new_path, strerror(errno));
            syslog(LOG_NOTICE, "Daemon shutting down");
            exit(EXIT_FAILURE);
          }
          syslog(LOG_INFO, "Created folder %s", new_path);
        }
        else
          closedir(tmp);
        browse_Folder(old_path, new_path, recursive, size_of_file);
        free(new_path);
        free(old_path);
      }
    }
  }
  if(closedir(path) == -1)
  {
    syslog(LOG_ERR, "Could not close folder %s; errno: %s", input_folder_path, strerror(errno));
    syslog(LOG_NOTICE, "Daemon shutting down");
    exit(EXIT_FAILURE);
  }
}
