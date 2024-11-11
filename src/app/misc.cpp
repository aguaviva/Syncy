#include <sys/stat.h>
#include <stdio.h>

bool read_string_from_file(const char *filename, char *buffer)
{
    char *d = buffer;
    FILE *file_ptr;
    file_ptr = fopen(filename, "r");
    if (file_ptr!=NULL) {
        while (1) 
        {
            signed char ch = fgetc(file_ptr);
            if(ch == EOF || ch == '\n')
                break;
            *d = ch;
            d++;            
        }
        *d = 0;

        // Closing the file
        fclose(file_ptr);
        return true;
    }
    
    return false;
}

bool copy_file(const char *source, const char *target)
{
    char            buffer[256];
    size_t          n;

    FILE *f1 = fopen(source, "rb");
    FILE *f2 = fopen(target, "wb");

    if (f1==NULL || f2==NULL)
        return false;

    bool res = true;
    while ((n = fread(buffer, sizeof(char), sizeof(buffer), f1)) > 0)
    {
        if (fwrite(buffer, sizeof(char), n, f2) != n)
        {
            res = false;
            break;
        }
    }
    fclose(f2);
    fclose(f1);
    return res;
}

bool file_exists(const char *filename)
{
    struct stat buffer;   
    bool file_exists = (stat (filename, &buffer) == 0); 
    return file_exists;
}

