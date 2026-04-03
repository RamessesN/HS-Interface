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
#include <limits.h>

#define MAX(a, b)          ((a) > (b) ? (a) : (b))
#define VAL_CONSTRAIN(val) ((val) > 255.0f ? 255 : ((val) < 0.0f ? 0 : (uint8_t)(val)))
#define LOG(s)             printf("%s\n", (s))
#define LOGF(fmt, ...)     printf(fmt "\n", __VA_ARGS__)
#define ERROR(s)           fprintf(stderr, "%s\n", (s))
#define ERRORF(fmt, ...)   fprintf(stderr, fmt "\n", __VA_ARGS__)
#define Image_Init()       calloc(1, sizeof(Image))

/* The RGB values of a pixel. */
typedef struct Pixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Pixel;

/* Q2: An image loaded from a file. */
typedef struct Image {
    int width;
    int height;
    int nvalues;
    Pixel *pixel_ptr;
} Image;

/* Q3a: Free a Image and its contents */
void free_image(Image *img) {
    if (img != NULL) {
        free(img->pixel_ptr);
        free(img);
    }
}

/* Q3b: Opens and reads an image file, returning a pointer to a new Image.
 * On error, prints an error message and returns NULL. */
Image *load_image(const char *filename) {
    // Open the file for reading in binary mode
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        ERROR("File openned failed.");
        return NULL;
    }

    Image *img = NULL;
    char file_format[4];
    int width, height, nvalues;

    // Header Validation
    if (fscanf(f, "%3s %d %d %d", file_format, &width, &height, &nvalues) != 4) {
        ERROR("Header read failed.");
        goto err;
    }

    // HQ8 format and nvalues Validation
    if (strcmp(file_format, "HQ8") != 0 || nvalues != 3) {
        ERROR("Invalid file format or nvalues.");
        goto err;
    }

    fgetc(f); // Ignore the whitespace character after the header

    img = Image_Init();
    if (img == NULL) {
        ERROR("Memory allocation failed for img.");
        goto err;
    }

    img->width = width;
    img->height = height;
    img->nvalues = nvalues;
    
    size_t num_pixels = (size_t)width * height;
    img->pixel_ptr = calloc(num_pixels, sizeof(Pixel));
    
    if (img->pixel_ptr == NULL) {
        ERROR("Memory allocation failed for pixels.");
        goto err;
    }

    // Read binary pixel data
    size_t pixels_read = fread(img->pixel_ptr, sizeof(Pixel), num_pixels, f);
    if (pixels_read != num_pixels) {
        ERROR("Reading pixel data failed.");
        goto err;
    }

    fclose(f);
    return img;

err:
    if (f != NULL)   fclose(f);
    if (img != NULL) free_image(img);
    return NULL;
}

/* Q3c: Write img to file filename. Return true on success, false on error. */
bool save_image(const Image *img, const char *filename) {
    if (img == NULL || img->pixel_ptr == NULL)
        return false;
    
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        ERROR("File write failed.");
        return false;
    }

    if (fprintf(f, "HQ8\n%d %d %d\n", img->width, img->height, img->nvalues) < 0) { // Write ASCII header
        ERROR("Writing ASCII header failed.");
        fclose(f);
        return false;
    }

    size_t num_pixels = (size_t)img->width * img->height;
    size_t pixels_written = fwrite(img->pixel_ptr, sizeof(Pixel), num_pixels, f); // Write binary pixel data

    if (pixels_written != num_pixels) {
        ERROR("Writing pixel data failed.");
        fclose(f);
        return false;
    }

    if (fclose(f) != 0) {
        ERROR("Closing file failed.");
        return false;
    }

    return true;
}

/* Q3d: Allocate a new Image and copy an existing Image's contents into it. */
Image *copy_image(const Image *source)
{
    if (source == NULL || source->pixel_ptr == NULL) 
        return NULL;

    Image *img = Image_Init();
    if (img == NULL) return NULL;

    img->width = source->width;
    img->height = source->height;
    img->nvalues = source->nvalues;

    size_t num_pixels = (size_t)img->width * img->height;
    img->pixel_ptr = calloc(num_pixels, sizeof(Pixel));
    
    if (img->pixel_ptr == NULL) {
        free(img);
        return NULL;
    }

    // for (size_t i = 0; i < num_pixels; i++) {
    //     img->pixel_ptr[i] = source->pixel_ptr[i];
    // }

    memcpy(img->pixel_ptr, source->pixel_ptr, num_pixels * sizeof(Pixel)); // More efficient
    return img;
}

/* Q4: Task BRIGHT
 * Multiply the RGB values of each pixel by a floating-point factor.
 * Returns a new Image containing the result, or NULL on error. */
Image *apply_BRIGHT(const Image *source, float factor)
{
    if (source == NULL) 
        return NULL;

    Image *out_img = copy_image(source);
    if (out_img == NULL) return NULL;

    size_t num_pixels = (size_t)out_img->width * out_img->height;

    for (size_t i = 0; i < num_pixels; i++) {
        float r = out_img->pixel_ptr[i].red * factor;
        float g = out_img->pixel_ptr[i].green * factor;
        float b = out_img->pixel_ptr[i].blue * factor;

        // Constrain the values to remain within the range 0-255
        out_img->pixel_ptr[i].red   = VAL_CONSTRAIN(r);
        out_img->pixel_ptr[i].green = VAL_CONSTRAIN(g);
        out_img->pixel_ptr[i].blue  = VAL_CONSTRAIN(b);
    }

    return out_img;
}

/* Q5: Task EDGE
 * Reports on the strength of horizontal pixel differences within each row.
 * Returns true on success, or false on error. */
bool apply_EDGE(const Image *source)
{
    if (source == NULL || source->pixel_ptr == NULL) 
        return false;

    int overall_min = INT_MAX;
    int overall_max = -1;

    for (int y = 0; y < source->height; y++) {
        int row_min = INT_MAX;
        int row_max = -1;

        for (int x = 0; x < source->width - 1; x++) {
            // Transfer 2D into 1D
            Pixel p1 = source->pixel_ptr[y * source->width + x];
            Pixel p2 = source->pixel_ptr[y * source->width + x + 1];

            #define diff(attr) abs((int)p1.attr - (int)p2.attr)
            int diff_r = diff(red);
            int diff_g = diff(green);
            int diff_b = diff(blue);
            #undef diff
            int total_diff = MAX(MAX(diff_r, diff_g), diff_b);

            if (total_diff < row_min) row_min = total_diff;
            if (total_diff > row_max) row_max = total_diff;
        }

        if (source->width <= 1) { // When no adjacent pixels to compare
            row_min = 0;
            row_max = 0;
        }

        LOGF("Row %d: minimum %d, maximum %d", y, row_min, row_max);

        if (row_min < overall_min)      
            overall_min = row_min;
        if (row_max > overall_max) 
            overall_max = row_max;
    }

    if (source->height == 0 || source->width <= 1) {
        overall_min = 0;
        overall_max = 0;
    }

    LOGF("Overall: minimum %d, maximum %d\n", overall_min, overall_max);

    return true;
}

/* Q6: Processing multiple input files */
int main(int argc, char *argv[])
{
    if (argc < 4 || argc % 2 != 0) {
        ERRORF("Usage: %s INPUT1 OUTPUT1 [INPUT2 OUTPUT2 ...] BRIGHTNESS_FACTOR", argv[0]);
        return EXIT_FAILURE;
    }

    float brightness_factor;
    const char *arg = argv[argc - 1];
    if (sscanf(arg, "%f", &brightness_factor) != 1) {
        ERRORF("A float is expected, but got %s", arg); // Set the last argument as the brightness factors
        return EXIT_FAILURE;
    }
    int num_pairs = (argc - 2) / 2;

    int state = EXIT_SUCCESS;

    Image **input_images = calloc(num_pairs, sizeof(Image *));
    if (input_images == NULL) {
        ERROR("Memory allocation failed for image array.");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < num_pairs; i++) {
        input_images[i] = load_image(argv[i * 2 + 1]); // Load all images into memory
        if (input_images[i] == NULL) {
            ERRORF("Failed to load image %d. Exiting.", i + 1);
            state = EXIT_FAILURE;
            goto err;
        }
    }

    for (int i = 0; i < num_pairs; i++) {
        /* Apply BRIGHT */
        Image *processed_img = apply_BRIGHT(input_images[i], brightness_factor);
        if (processed_img == NULL) {
            ERROR("Processing BRIGHT failed.");
            state = EXIT_FAILURE;
            goto err;
        }

        /* Apply EDGE */
        LOGF("EDGE report for: %s", argv[i * 2 + 1]);
        apply_EDGE(processed_img);

        if (!save_image(processed_img, argv[i * 2 + 2])) {
            ERRORF("Saving image to %s failed.", argv[i * 2 + 2]);
            free_image(processed_img);
            state = EXIT_FAILURE;
            goto err;
        }

        free_image(processed_img);
    }

err:
    if (input_images != NULL) {
        for (int i = 0; i < num_pairs; i++) {
            free_image(input_images[i]);
        }
        free(input_images);
    }

    return state;
}
