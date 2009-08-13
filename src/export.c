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

/**
 * @defgroup data Export functions
 * The module contains functions for exporting data computed by 
 * Malheur to external format such as plain text and HTML documents.
 * @author Konrad Rieck (rieck@cs.tu-berlin.de)
 * @{
 */

#include "config.h"
#include "common.h"
#include "farray.h"
#include "util.h"
#include "export.h"
#include "mconfig.h"

/* External variables */
extern int verbose;
extern config_t cfg;

/**
 * Exports a kernel matrix to a file
 * @param d Pointer to matrix
 * @param fa Feature vector array
 * @param file File name 
 */
void export_kernel(double *d, farray_t *fa, char *file)
{
    assert(d && fa && file);
    int i,j;

    if (verbose > 0)
        printf("Exporting kernel matrix to '%s'.\n", file);

    FILE *f = fopen(file, "w");
    if (!f) {
        error("Could not create file '%s'.", file);
        return;
    }

    for (i = 0; i < fa->len; i++) {
        fprintf(f, "%s:", fa->x[i]->src);
        for (j = 0; j < fa->len; j++)
            fprintf(f, " %g", d[i * fa->len + j]);
        fprintf(f, "\n");
    }

    fclose(f);
}
 
/**
 * Extracts a CWSandbox URL from a filename. The function uses a static 
 * buffer and thus is not thread-safe.
 * @param f File name
 * @return URL to CWSandbox report
 */
static char *cwsandbox_url(char *f)
{
    static char buf[1024];
    char *ptr = f + strlen(f) - 1;
    int sid, aid;
    
    /* Determine basename */
    while(ptr != f && *(ptr - 1) != '/')
        ptr--;
    
    /* Get sample and analysis id */
    sscanf(ptr, "%d_%d.%*s\n", &sid, &aid);
    
    /* Construct URL */
    snprintf(buf, 1024, "%s&analysisid=%d", CWS_URL, aid);
    return buf;
}


/**
 * Exports a structure of prototypes to a file
 * @param p Prototype structure
 * @param fa Feature vector array
 * @param file File name
 */
void export_proto(proto_t *p, farray_t *fa, char *file)
{
    assert(p && fa && file);
    int i, j, k = 0, n = 0, x = 0;
    
    if (verbose > 0)
        printf("Exporting prototypes to '%s'.\n", file);
        
    FILE *f = fopen(file, "w");
    if (!f) {
        error("Could not open '%s' for writing", file);
        return;
    }
    
    /* Write generic */
    fprintf(f, "<html><body>%s<h1>Prototypes</h1>", BODY);

    fprintf(f, TABS);
    fprintf(f, TS "Report source:" TM "%s", p->protos->src);    
    fprintf(f, TS "Number of prototypes:" TM "%lu" TE, p->protos->len);
    fprintf(f, TS "Number of total reports:" TM "%lu" TE, p->alen);
    fprintf(f, TS "Compression ratio:" TM "%4.1f%%" TE, 
            (1.0 - p->protos->len / (double) fa->len) * 100);
    fprintf(f, TABE);
    
    /* Write configuration */
    fprintf(f, "<h2>Configuration</h2><pre>");
    config_fprint(f, &cfg);
    fprintf(f, "</pre>");    
    
    /* Write prototypes and assignments */
    fprintf(f, "<h2>Assignments</h2><ol>\n");
    for (i = 0; i < p->protos->len; i++) {
    
        /* Count members per prototype */
        for (j = 0, n = 0; j < p->alen; j++) {
            if ((p->assign[j] & PA_ASSIGN_MASK) == i) {
                n++;
                if ((p->assign[j] & PA_PROTO_MASK) != 0)
                    k = j;
            }
        }

        fprintf(f, "<li><a href='%s'><b>Prototype</b></a> "
                "(members: %d, label: %s, index: %d)<br>\n", 
                cwsandbox_url(p->protos->x[i]->src), n, 
                farray_get_label(p->protos, i), k);
               
        /* Write list of assignments */
        for (j = 0, x = 0; j < p->alen; j++) {
            if ((p->assign[j] & PA_ASSIGN_MASK) != i)
                continue;
                
            fprintf(f, "<a href='%s' style='text-decoration: none;'>", 
                    cwsandbox_url(fa->x[j]->src));
            if (x && x == fa->y[j]) 
                fprintf(f, "&middot;");
            else
                fprintf(f, "%s", farray_get_label(fa, j));
            x = fa->y[j];
            fprintf(f, "</a> ");
        }
        fprintf(f, "<br>\n");
    }
    fprintf(f,"</ol></body></html>\n");
    fclose(f);
}

/** @} */
 
