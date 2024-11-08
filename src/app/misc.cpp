
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
