/* This coursework specification, and the example code provided during the
 * course, is Copyright 2026 Heriot-Watt University.
 * Distributing this coursework specification or your solution to it outside
 * the university is academic misconduct and a violation of copyright law. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* The RGB values of a pixel. */
struct Pixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

/* Q2: An image loaded from a file. */
struct Image {
    int width;
    int height;
    int nvalues;
    struct Pixel *pixels;
};

/* Q3a: Free a struct Image and its contents */
void free_image(struct Image *img)
{
    if (img != NULL) {
        if (img->pixels != NULL) {
            free(img->pixels);
        }
        free(img);
    }
}

/* Q3b: Opens and reads an image file, returning a pointer to a new struct Image.
 * On error, prints an error message and returns NULL. */
struct Image *load_image(const char *filename)
{
    /* Open the file for reading in binary mode */
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        fprintf(stderr, "File %s could not be opened.\n", filename);
        return NULL;
    }

    char magic[4];
    int width, height, nvalues;

    /* Read the header */
    if (fscanf(f, "%3s %d %d %d", magic, &width, &height, &nvalues) != 4) {
        fprintf(stderr, "Error reading header from %s.\n", filename);
        fclose(f);
        return NULL;
    }

    /* Validate the HQ8 format and nvalues */
    if (strcmp(magic, "HQ8") != 0 || nvalues != 3) {
        fprintf(stderr, "Invalid image format or nvalues in %s.\n", filename);
        fclose(f);
        return NULL;
    }

    /* Consume the single whitespace character after the header */
    fgetc(f);

    /* Allocate the Image object, and read the image from the file */
    struct Image *img = malloc(sizeof(struct Image));
    if (img == NULL) {
        fprintf(stderr, "Memory allocation failed for Image object.\n");
        fclose(f);
        return NULL;
    }

    img->width = width;
    img->height = height;
    img->nvalues = nvalues;
    
    size_t num_pixels = (size_t)width * height;
    img->pixels = malloc(num_pixels * sizeof(struct Pixel));
    
    if (img->pixels == NULL) {
        fprintf(stderr, "Memory allocation failed for pixels in %s.\n", filename);
        free(img);
        fclose(f);
        return NULL;
    }

    /* Read binary pixel data */
    size_t pixels_read = fread(img->pixels, sizeof(struct Pixel), num_pixels, f);
    if (pixels_read != num_pixels) {
        fprintf(stderr, "Error reading pixel data from %s.\n", filename);
        free_image(img);
        fclose(f);
        return NULL;
    }

    fclose(f);
    return img;
}

/* Q3c: Write img to file filename. Return true on success, false on error. */
bool save_image(const struct Image *img, const char *filename)
{
    if (img == NULL || img->pixels == NULL)
        return false;
    
    /* Open the file for writing in binary mode */
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        fprintf(stderr, "File %s could not be opened for writing.\n", filename);
        return false;
    }

    /* Write ASCII header */
    fprintf(f, "HQ8\n%d %d %d\n", img->width, img->height, img->nvalues);

    /* Write binary pixel data */
    size_t num_pixels = (size_t)img->width * img->height;
    size_t pixels_written = fwrite(img->pixels, sizeof(struct Pixel), num_pixels, f);
    
    fclose(f);

    if (pixels_written != num_pixels) {
        fprintf(stderr, "Error writing pixel data to %s.\n", filename);
        return false;
    }

    return true;
}

/* Q3d: Allocate a new struct Image and copy an existing struct Image's contents into it. */
struct Image *copy_image(const struct Image *source)
{
    if (source == NULL || source->pixels == NULL) 
        return NULL;

    struct Image *img = malloc(sizeof(struct Image));
    if (img == NULL) return NULL;

    img->width = source->width;
    img->height = source->height;
    img->nvalues = source->nvalues;

    size_t num_pixels = (size_t)img->width * img->height;
    img->pixels = malloc(num_pixels * sizeof(struct Pixel));
    
    if (img->pixels == NULL) {
        free(img);
        return NULL;
    }

    // for (size_t i = 0; i < num_pixels; i++) {
    //     img->pixels[i] = source->pixels[i];
    // }

    /* Much more efficient than copying pixel by pixel in a loop */
    memcpy(img->pixels, source->pixels, num_pixels * sizeof(struct Pixel));

    return img;
}

/* Q4: Task BRIGHT
 * Multiply the RGB values of each pixel by a floating-point factor.
 * Returns a new struct Image containing the result, or NULL on error. */
struct Image *apply_BRIGHT(const struct Image *source, float factor)
{
    if (source == NULL) 
        return NULL;

    struct Image *out_img = copy_image(source);
    if (out_img == NULL) 
        return NULL;

    size_t num_pixels = (size_t)out_img->width * out_img->height;

    /* Process sequentially to maximize cache efficiency */
    for (size_t i = 0; i < num_pixels; i++) {
        float r = out_img->pixels[i].red * factor;
        float g = out_img->pixels[i].green * factor;
        float b = out_img->pixels[i].blue * factor;

        /* Constrain the values to remain within the range 0-255 */
        out_img->pixels[i].red   = (r > 255.0f) ? 255 : ((r < 0.0f) ? 0 : (uint8_t)r);
        out_img->pixels[i].green = (g > 255.0f) ? 255 : ((g < 0.0f) ? 0 : (uint8_t)g);
        out_img->pixels[i].blue  = (b > 255.0f) ? 255 : ((b < 0.0f) ? 0 : (uint8_t)b);
    }

    return out_img;
}

/* Q5: Task EDGE
 * Reports on the strength of horizontal pixel differences within each row.
 * Returns true on success, or false on error. */
bool apply_EDGE(const struct Image *source)
{
    if (source == NULL || source->pixels == NULL) 
        return false;

    int overall_min = 255 * 3 + 1; /* Max possible difference is 255 per channel */
    int overall_max = -1;

    for (int y = 0; y < source->height; y++) {
        int row_min = 255 * 3 + 1;
        int row_max = -1;

        for (int x = 0; x < source->width - 1; x++) {
            /* Compute 1D index from 2D coordinates */
            struct Pixel p1 = source->pixels[y * source->width + x];
            struct Pixel p2 = source->pixels[y * source->width + x + 1];

            /* Sum of abs differences for the RGB values */
            int diff_r = abs((int)p1.red - (int)p2.red);
            int diff_g = abs((int)p1.green - (int)p2.green);
            int diff_b = abs((int)p1.blue - (int)p2.blue);
            int total_diff = diff_r + diff_g + diff_b;

            if (total_diff < row_min) row_min = total_diff;
            if (total_diff > row_max) row_max = total_diff;
        }

        /* Handle edge case where width is 1 - no adjacent pixels to compare */
        if (source->width <= 1) {
            row_min = 0;
            row_max = 0;
        }

        printf("Row %d: minimum %d, maximum %d\n", y, row_min, row_max);

        if (row_min < overall_min)      
            overall_min = row_min;
        if (row_max > overall_max) 
            overall_max = row_max;
    }

    if (source->height == 0 || source->width <= 1) {
        overall_min = 0;
        overall_max = 0;
    }

    printf("Overall: minimum %d, maximum %d\n", overall_min, overall_max);

    return true;
}

/* Q6: Processing multiple input files */
int main(int argc, char *argv[])
{
    /* Check command-line arguments */
    if (argc < 4 || argc % 2 != 0) {
        fprintf(stderr, "Usage: %s INPUT1 OUTPUT1 [INPUT2 OUTPUT2 ...] BRIGHTNESS_FACTOR\n", argv[0]);
        return 1;
    }

    /* Set the last argument as the brightness factor */
    float brightness_factor = (float)atof(argv[argc - 1]);
    int num_pairs = (argc - 2) / 2;

    /* Allocate array to keep track of all loaded input images */
    struct Image **input_images = malloc(num_pairs * sizeof(struct Image *));
    if (input_images == NULL) {
        fprintf(stderr, "Memory allocation failed for image array.\n");
        return 1;
    }

    /* Step 1 - Load ALL images into memory */
    bool load_success = true;
    for (int i = 0; i < num_pairs; i++) {
        input_images[i] = load_image(argv[i * 2 + 1]);
        if (input_images[i] == NULL) {
            load_success = false;
            break; 
        }
    }

    /* If any image failed to load, free what was loaded and abort */
    if (!load_success) {
        for (int i = 0; i < num_pairs; i++) {
            free_image(input_images[i]); /* to handle NULL safely */
        }
        free(input_images);
        return 1;
    }

    /* Step 2 - Process and save all images */
    for (int i = 0; i < num_pairs; i++) {
        /* Apply BRIGHT */
        struct Image *processed_img = apply_BRIGHT(input_images[i], brightness_factor);
        if (processed_img == NULL) {
            fprintf(stderr, "Processing BRIGHT failed for image %d.\n", i + 1);
            continue; /* Skip to next if processing fails */
        }

        /* Apply EDGE */
        printf("EDGE report for: %s\n", argv[i * 2 + 1]); /* Prints to stdout */
        apply_EDGE(processed_img);

        /* Save Image */
        if (!save_image(processed_img, argv[i * 2 + 2])) {
            fprintf(stderr, "Saving image to %s failed.\n", argv[i * 2 + 2]);
        }

        /* Free the processed image */
        free_image(processed_img);
    }

    /* Step 3 - Cleanup original input images */
    for (int i = 0; i < num_pairs; i++) {
        free_image(input_images[i]);
    }
    free(input_images);

    return 0;
}