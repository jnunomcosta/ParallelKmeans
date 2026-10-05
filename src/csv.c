#define _POSIX_C_SOURCE 200809L

#include "kmeans/kmeans.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

int km_dataset_save_csv(const km_dataset *ds, const char *path)
{
    if (!ds || !path || !ds->points || ds->n == 0 || ds->dim == 0)
        return KM_ERR_ARG;
    FILE *f = fopen(path, "w");
    if (!f)
        return KM_ERR_IO;
    for (size_t i = 0; i < ds->n; i++)
    {
        for (size_t d = 0; d < ds->dim; d++)
            fprintf(f, d ? ",%.9g" : "%.9g", (double)ds->points[i * ds->dim + d]);
        fputc('\n', f);
    }
    int err = ferror(f) ? KM_ERR_IO : KM_OK;
    if (fclose(f) != 0)
        err = KM_ERR_IO;
    return err;
}

static int is_blank(const char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    return *s == '\0';
}

/* Parses one line into *vals (growing it as needed) and returns the column count.
 * Returns 0 if the first field is not a number, KM_ERR_PARSE on a bad later field. */
static int parse_line(const char *line, float **vals, size_t *cap, size_t *cols)
{
    size_t c = 0;
    const char *s = line;
    for (;;)
    {
        char *end;
        float v = strtof(s, &end);
        if (end == s)
            return c == 0 ? 0 : KM_ERR_PARSE;
        while (*end && isspace((unsigned char)*end))
            end++;
        if (*end != ',' && *end != '\0')
            return c == 0 ? 0 : KM_ERR_PARSE;
        if (c == *cap)
        {
            size_t ncap = *cap ? *cap * 2 : 8;
            float *p = realloc(*vals, ncap * sizeof(float));
            if (!p)
                return KM_ERR_NOMEM;
            *vals = p;
            *cap = ncap;
        }
        (*vals)[c++] = v;
        if (*end == '\0')
            break;
        s = end + 1;
    }
    *cols = c;
    return 1;
}

int km_dataset_load_csv(km_dataset *ds, const char *path)
{
    if (!ds || !path)
        return KM_ERR_ARG;
    FILE *f = fopen(path, "r");
    if (!f)
        return KM_ERR_IO;

    float *data = NULL; /* all values, row-major */
    size_t len = 0, cap = 0;
    float *row = NULL;
    size_t row_cap = 0, dim = 0, rows = 0;
    char *line = NULL;
    size_t line_cap = 0;
    int err = KM_OK;
    bool first = true;

    while (getline(&line, &line_cap, f) != -1)
    {
        if (is_blank(line))
            continue;
        size_t cols = 0;
        int r = parse_line(line, &row, &row_cap, &cols);
        if (r == 0 && first)
        { /* header line */
            first = false;
            continue;
        }
        first = false;
        if (r <= 0)
        {
            err = r == 0 ? KM_ERR_PARSE : r;
            break;
        }
        if (dim == 0)
            dim = cols;
        else if (cols != dim)
        {
            err = KM_ERR_PARSE;
            break;
        }
        if (len + dim > cap)
        {
            size_t ncap = cap ? cap * 2 : 1024;
            while (ncap < len + dim)
                ncap *= 2;
            float *p = realloc(data, ncap * sizeof(float));
            if (!p)
            {
                err = KM_ERR_NOMEM;
                break;
            }
            data = p;
            cap = ncap;
        }
        for (size_t d = 0; d < dim; d++)
            data[len++] = row[d];
        rows++;
    }
    if (err == KM_OK && ferror(f))
        err = KM_ERR_IO;
    if (err == KM_OK && rows == 0)
        err = KM_ERR_PARSE;
    fclose(f);
    free(line);
    free(row);
    if (err != KM_OK)
    {
        free(data);
        return err;
    }
    ds->points = data;
    ds->n = rows;
    ds->dim = dim;
    return KM_OK;
}
