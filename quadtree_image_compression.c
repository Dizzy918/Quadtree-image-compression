#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define THRESHOLD 30

typedef struct quadtree_node {
    int x;
    int y;
    int size;
    int value[3];
    struct quadtree_node* child[4];
} quadtree_node;

int width;
int height;
int ***image = NULL;
int ***output = NULL;

int*** allocating_memory_for_image(int height, int width){
    int ***array = malloc(height * sizeof(int**));
    if(!array){
        printf("Error allocating memory");
        exit(1);
    }

    for (int i = 0; i < height; i++){

        array[i] = malloc(width * sizeof(int*));
        if(!array[i]){
            printf("Error allocating memory");
            exit(1);
        }

        for (int j = 0; j < width; j++){
            array[i][j] = malloc(3 * sizeof(int));
            if(!array[i][j]){
                printf("Error allocating memory");
                exit(1);
            }
        }
    }

    return array;
}

void free_memory_of_image(int ***array, int height, int width){
    for (int i = 0; i < height; i++){

        for (int j = 0; j < width; j++) free(array[i][j]);
        
        free(array[i]);
    }

    free(array);
}

quadtree_node* quadtree_builder(int row_index, int column_index, int size){
    quadtree_node* node = malloc(sizeof(quadtree_node));
    node->x = row_index;
    node->y = column_index;
    node->size = size;

    for (int i = 0; i < 4; i++) node->child[i] = NULL;

    int minimum_value[3] = {255,255,255};
    int maximum_value[3] = {0,0,0};
    int sum_of_values[3] = {0,0,0};
    int count = 0;

    for (int row = row_index; row < row_index + size && row < height; row++){
        for (int column = column_index; column < column_index + size && column < width; column++){
            for (int channel = 0; channel < 3; channel++){
                int value = image[row][column][channel];
                if (value < minimum_value[channel]) minimum_value[channel] = value;
                if (value > maximum_value[channel]) maximum_value[channel] = value;
                sum_of_values[channel] += value;
            }
            count++;
        }
    }

    int is_leaf = 1;

    for (int channel = 0; channel < 3; channel++){
        if (maximum_value[channel] - minimum_value[channel] > THRESHOLD){
            is_leaf = 0;
            break;
        }
    }

    if (is_leaf || size <= 1 || count == 0){
        for (int channel = 0; channel < 3; channel++) if (count != 0) node->value[channel] = sum_of_values[channel] / count;
        else {
            node->value[channel] = sum_of_values[channel];
        }
    } else {
        int half_size = size / 2;
        for (int channel = 0; channel < 3; channel++) node->value[channel] = -1;
        node->child[0] = quadtree_builder(row_index, column_index, half_size);
        node->child[1] = quadtree_builder(row_index + half_size, column_index, half_size);
        node->child[2] = quadtree_builder(row_index, column_index + half_size, half_size);
        node->child[3] = quadtree_builder(row_index + half_size, column_index + half_size, half_size);
    }
    return node;
}

void set_pixel(int ***destination, int row, int column, int rgb[3]){
    for (int channel = 0; channel < 3; channel++) destination[row][column][channel] = rgb[channel];
}

void copy_image(const uint8_t* copy_image, int width,   int height, int channels){
    for (int row = 0; row < height; row++){
        for (int column = 0; column < width; column++){
            int index = row * width * channels + column * channels;
            int rgb[3];
            if(channels == 4 && copy_image[index + 3] == 0){
                rgb[0] = 255;
                rgb[1] = 255;
                rgb[2] = 255;
            } else {
                for (int channel = 0; channel < 3; channel++) rgb[channel] = copy_image[index + channel];
            }
            
            set_pixel(image, row, column, rgb);
        }
    }
}

void fill_region(int ***destination, int start_of_row, int start_of_column, int size, int rgb[3]){
    for (int row = start_of_row; row < start_of_row + size && row < height; row++)
        for (int column = start_of_column; column < start_of_column + size && column < width; column++) set_pixel(destination, row, column, rgb);
}

void fill_output(quadtree_node* node){
    if (!node) return;
    if (!node->child[0]) fill_region(output, node->x, node->y, node->size, node->value);
    else {
        for (int i = 0; i < 4; i++) fill_output(node->child[i]);
    }
}

void free_tree(quadtree_node* node){
    if (!node) return;

    for (int i = 0; i < 4; i++) free_tree(node->child[i]);

    free(node);
}

int main(int argc, char* argv[]){
    if (argc != 3){
        printf("Usage: %s input_image output_image.png\n", argv[0]);
        return 1;
    }

    int channels;

    uint8_t* new_image = stbi_load(argv[1], &width, &height, &channels, 4);

    if (!new_image){
        printf("Failed to load image: %s\n", argv[1]);
        return 1;
    }

    image = allocating_memory_for_image(height, width);
    output = allocating_memory_for_image(height, width);

    copy_image(new_image, width, height, 4);
    stbi_image_free(new_image);

    int size = 1;
    while (size < width || size < height) size *= 2;

    quadtree_node* root = quadtree_builder(0, 0, size);
    fill_output(root);
    free_tree(root);

    size_t image_size = (size_t)width * (size_t)height * 3;
    unsigned char* result = malloc(image_size);

    for (int row = 0; row < height; row++)
        for (int column = 0; column < width; column++)
            for (int channel = 0; channel < 3; channel++) result[(row * width + column) * 3 + channel] = (unsigned char)output[row][column][channel];

    if (!stbi_write_png(argv[2], width, height, 3, result, width * 3)){
        printf("Failed to save image: %s\n", argv[2]);
        free(result);
        free_memory_of_image(image, height, width);
        free_memory_of_image(output, height, width);
        return 1;
    }

    printf("Compressed image saved as: %s\n", argv[2]);
    free(result);
    free_memory_of_image(image, height, width);
    free_memory_of_image(output, height, width);
    return 0;
}