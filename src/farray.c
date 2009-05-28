/*
 * MALHEUR - Automatic Malware Analysis on Steroids
 * Copyright (c) 2009 Konrad Rieck (rieck@cs.tu-berlin.de)
 * Berlin Institute of Technology (TU Berlin).
 * --
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.  This program is distributed without any
 * warranty. See the GNU General Public License for more details. 
 * --
 */

#include "config.h"
#include "common.h"
#include "farray.h"
#include "fvec.h"
#include "fio.h"
#include "util.h"

/* External variables */
extern int verbose;

/**
 * Adds a label to the table of labels stored in an array.
 * @param name label name
 * @return index of label 
 */ 
static int label_add(farray_t *fa, char *name)
{
    label_t *entry;
    assert(fa && name);

    /* Check if label is known */
    HASH_FIND(hname, fa->label_name, name, strlen(name), entry);    
    if (entry) 
        return entry->index;
        
    /* Create new label */
    entry = malloc(sizeof(label_t));
    entry->name = strdup(name); 
    entry->index = hash_string(name);
             
    /* Add label to both tables */
    HASH_ADD(hindex, fa->label_index, index, 
             sizeof(int), entry);
    HASH_ADD_KEYPTR(hname, fa->label_name, entry->name, 
                    strlen(entry->name), entry);     
                    
    /* Update memory */
    fa->mem += sizeof(label_t) + sizeof(name);

    /* Return new index */
    return entry->index;
}

/**
 * Creates and allocates an empty array of feature vectors
 * @return empty array
 */
farray_t *farray_create()
{
    farray_t *fa = calloc(1, sizeof(farray_t));
    if (!fa) {
        error("Could not allocate array of feature vectors");
        return NULL;
    }

    /* Init elements of array */
    fa->len = 0;
    fa->mem = sizeof(farray_t);
    
    return fa;
} 


/**
 * Destroys an array of feature vectors
 * @param a array of feature vectors
 */
void farray_destroy(farray_t *fa)
{
    if (!fa)
        return;
    
    /* Free feature vectors */
    if (fa->x) {
        for (int i = 0; i < fa->len; i++)
            fvec_destroy(fa->x[i]);
        free(fa->x);
    }

    /* Free label indices */
    if (fa->y)
        free(fa->y);
        
    /* Free lable table */
    while(fa->label_name) {
        label_t *current = fa->label_name;        
        HASH_DELETE(hname, fa->label_name, current);
        free(current->name);
        free(current);           
    }    
       
    free(fa);
}

/**
 * Adds a feature vector to the array
 * @param x Feature vector 
 * @param y Label of feature vector 
 */
void farray_add(farray_t *fa, fvec_t *fv, char *label)
{
    assert(fa && fv && label);

    /* Expand size of array */
    if (fa->len % BLOCK_SIZE == 0) {
        int l = fa->len + BLOCK_SIZE;
        fa->x = realloc(fa->x, l * sizeof(fvec_t *));
        fa->y = realloc(fa->y, l * sizeof(int));
        fa->mem += BLOCK_SIZE * (sizeof(fvec_t *) + sizeof(int));
        if (!fa->x || !fa->y) {
            error("Could not re-size feature array.");
            farray_destroy(fa);
            return;
        }
    }
    
    /* Update table */
    fa->x[fa->len] = fv;
    fa->y[fa->len] = label_add(fa, label);
    fa->len++;    
    fa->mem += fv->mem;
}

/**
 * Extracts an array of feature vectors from a directory. The function 
 * loads and converts files in the given directory. It does not process
 * subdirectories recursively.
 * @param dir directory containing file.
 */
farray_t *farray_extract_dir(char *dir)
{
    char *x;
    struct dirent *dp;
    assert(dir);

    /* Allocate empty array */
    farray_t *fa = farray_create();
    if (!fa) 
        return NULL;
    
    /* Open directory */    
    long n = fio_count_files(dir);
    DIR *d = opendir(dir);
    if (!d) {
        farray_destroy(fa);
        error("Could not open directory '%s'", dir);
        return NULL;
    }
    
    /* Loop over directory entries */
    while((dp = readdir(d)) != NULL) {
        if (dp->d_type != DT_REG) 
            continue;
    
        /* Load file contents */
        x = fio_load_file(dir, dp->d_name);
    
        /* Preprocess and extract feature vector*/
        x = fio_preproc(x);
        fvec_t *fv = fvec_extract(x, strlen(x));
        free(x);

        /* Extract label from name */
        x = file_suffix(dp->d_name);
        
        /* Add feature vector to array */
        farray_add(fa, fv, x);
        
        if (verbose > 0)
            prog_bar(0, n, fa->len);
    }   
    if (verbose > 0)
        printf("\n");
        
    closedir(d);
    return fa;
}

/**
 * Prints a feature array
 * @param feature array
 */
void farray_print(farray_t *fa)
{
    assert(fa);
    int i;
    label_t *entry;

    printf("feature array [len: %lu, labels: %d, ", fa->len, 
           HASH_CNT(hname, fa->label_name));
    printf("%.2fMb, %p/%p/%p]\n", fa->mem / 1e6,
           (void *) fa, (void *) fa->x, (void *) fa->y);
           
    if (verbose < 2)
        return;
    
    for (i = 0; i < fa->len; i++) {
        fvec_print(fa->x[i]);
        HASH_FIND(hindex, fa->label_index, &fa->y[i], sizeof(int), entry);    
        printf("  label: %s, index %d\n", entry->name, fa->y[i]);        
    }   
}

/**
 * Saves an array of feature vectors to a file stream
 * @param fa Array of feature vectors
 * @param z Stream pointer
 */
void farray_save(farray_t *fa, gzFile *z)
{
    assert(fa && z);
    int i;
    label_t *entry;

    gzprintf(z, "feature array: len=%lu, labels=%d, mem=%lu\n", 
            fa->len, HASH_CNT(hname, fa->label_name), fa->mem);
            
    for (i = 0; i < fa->len; i++) {
        fvec_save(fa->x[i], z);
        HASH_FIND(hindex, fa->label_index, &fa->y[i], sizeof(int), entry);   
        gzprintf(z, "  label=%s\n", entry->name);
    }    
}

/**
 * Loads an array of feature vector form a file stream
 * @param z Stream point
 * @return  Array of feature vectors
*/
farray_t *farray_load(gzFile *z)
{
    assert(z);
    char buf[512], str[512];
    long len, mem;
    int lab, r, i;

    /* Allocate feature array */
    farray_t *f = farray_create();
    if (!f) 
        return NULL;

    gzgets(z, buf, 512);
    r = sscanf(buf, "feature array: len=%lu, labels=%d, mem=%lu\n", 
              (unsigned long *) &len, (int *) &lab, 
              (unsigned long *) &mem);              
    if (r != 3)  {
        error("Could not parse feature array");
        farray_destroy(f);
        return NULL;
    }
    
    /* Load contents */
    for (i = 0; i < len; i++) {
        /* Load feature vector */
        fvec_t *fv = fvec_load(z);
        
        /* Load labels */
        gzgets(z, buf, 512);
        r = sscanf(buf, "  label=%s\n", str);
        if (r != 1) {
            error("Could not parse feature vector contents");
            farray_destroy(f);
            return NULL;
        }
        
        /* Add to array */
        farray_add(f, fv, str); 
    }           
    return f;
}

 