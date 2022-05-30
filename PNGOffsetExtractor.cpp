#include <iostream>
#include <filesystem>
#include <vector>
#include <cstring>

#include <png.h>
#include <setjmp.h>

namespace std {
    namespace fs=filesystem;
}

constexpr std::byte operator ""_b(unsigned long long int u){
    return static_cast<std::byte>(u);
}

std::vector<std::fs::path> files;

std::byte png_magic[4]={0x89_b,0x50_b,0x4e_b,0x47_b};

bool f_is_png(FILE * f){
    std::byte buffer[4];
    return (fread(buffer,1,4,f)==4)&&(buffer[0]==png_magic[0])&&(buffer[1]==png_magic[1])&&(buffer[2]==png_magic[2])&&(buffer[3]==png_magic[3]);
}

namespace sprite {
    int32_t offset_x;
    int32_t offset_y;
    bool found_offset;
}

char offset_chunk[5]={'g','r','A','b','\0'};

int libpng_chunk_callback(png_structp png_ptr, png_unknown_chunkp chunk){
    if(memcmp(offset_chunk,chunk->name,5)==0){
        if(chunk->size==8){
            sprite::found_offset=true;
            sprite::offset_x=(chunk->data[0]<<24)|(chunk->data[1]<<16)|(chunk->data[2]<<8)|(chunk->data[3]);
            sprite::offset_y=(chunk->data[4]<<24)|(chunk->data[5]<<16)|(chunk->data[6]<<8)|(chunk->data[7]);
        }
    }
    return 1;
}

void handle_png(std::fs::path file_path){
    if(!std::fs::is_regular_file(file_path)){
        return;
    }
    FILE * f=fopen(file_path.string().c_str(),"rb");
    if(!f){
        return;
    }
    if(f_is_png(f)){
        sprite::found_offset=false;
        png_structp png_s = png_create_read_struct(
                    PNG_LIBPNG_VER_STRING,
                    NULL,
                    NULL,
                    NULL
                 );
        if(!png_s){
            fclose(f);
            return;
        }
        png_infop png_i = png_create_info_struct(png_s);
        if(!png_i){
            png_destroy_read_struct(&png_s,NULL,NULL);
            fclose(f);
            return;
        }
        png_init_io(png_s,f);
        png_set_sig_bytes(png_s,4);
        png_set_read_user_chunk_fn(png_s,NULL,libpng_chunk_callback);
        
        png_read_info(png_s,png_i);
        
        if(setjmp(png_jmpbuf(png_s))){
            png_destroy_read_struct(&png_s,&png_i,NULL);
            fclose(f);
            return;
        }
        
        if(sprite::found_offset){//ignore sprites without offset
            png_uint_32 width;
            png_uint_32 height;
            int bit_depth, color_type;
            png_get_IHDR(png_s,png_i,&width,&height,&bit_depth,&color_type,NULL,NULL,NULL);
            std::string name=file_path.stem().string();
            fprintf(stdout,"Sprite %s, %d, %d\n",name.c_str(),width,height);
            fprintf(stdout,"{\n");
            fprintf(stdout,"    Patch %s, 0, 0\n",name.c_str());
            fprintf(stdout,"    Offset %d, %d\n",sprite::offset_x,sprite::offset_y);
            fprintf(stdout,"}\n\n");
        }
        png_destroy_read_struct(&png_s,&png_i,NULL);
    }
    fclose(f);
}

void iterate_folder(std::fs::path folder){
    for(auto &entry:std::filesystem::directory_iterator(folder)){
        if(std::fs::is_directory(entry)){
            iterate_folder(entry);
        }else{
            handle_png(entry);
        }
    }
}

int main(int argc,char ** argv) {
    const char * folder="Sprites";
    if(argc==2){
        folder=argv[1];
    }
    if(!std::fs::exists(folder)) {
        fprintf(stderr,"Could not find '%s' folder\n",folder);
        return 1;
    } else if(!std::fs::is_directory(folder)) {
        fprintf(stderr,"'%s' is not a folder\n",folder);
        return 1;
    }
    iterate_folder(folder);
    return 0;
}
