
//server file record
typedef struct {
    //int inode;//file identifier
    int index;//offset in a file
    int *owner;//many people owns file block
}file_block;

//file infomation
typedef struct 
{
    char *filename;
    int file_chunks;
    file_block block_location;
}file;







