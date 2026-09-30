//
//  Created by Behnam Dezfouli
//  CSEN, Santa Clara University
//
//
// This program offers two options to the user:
// -- Option 1: enables the user to copy a file to another file. 
// The user needs to enter the name of the source and destination files.
// -- Option 2: enables the user to measure the performance of file copy. 
// In addition to the name of source and destination files, the user needs to enter the maximum source file size as well as the step size to increment the source file size.
//
//
// double copier(FILE *s_file, FILE *d_file)
// Precondition: s_file is a pointer to a file opened in read mode, d_file is a file pointer opened in write mode
// Postcondition: Copies the contents of the file pointed to by s_file to the file pointed to by d_file
//


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>

// Set the buffer size and number of timing trials used by the program.
#define BUFFER_SIZE 4096
#define NUM_TRIALS 5

// Helper function to create/resize the source file to a specific size in bytes
void generate_dummy_file(const char *filename, size_t size_in_bytes) {
    // Open the source file and prepare sample data for writing.
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("Error creating source file");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = (char)(i % 256);
    }

    size_t bytes_written = 0;
    // Write data until the file reaches the requested size.
    while (bytes_written < size_in_bytes) {
        size_t bytes_to_write = BUFFER_SIZE;
        if (size_in_bytes - bytes_written < BUFFER_SIZE) {
            bytes_to_write = size_in_bytes - bytes_written;
        }
        fwrite(buffer, 1, bytes_to_write, fp);
        bytes_written += bytes_to_write;
    }

    fclose(fp);
}

// Function to copy source file to destination file using standard C library functions
void copy_file(const char *src_path, const char *dst_path) {
    // Open both files before transferring data between them.
    FILE *src = fopen(src_path, "rb");
    if (src == NULL) {
        perror("Error opening source file for copying");
        exit(EXIT_FAILURE);
    }

    FILE *dst = fopen(dst_path, "wb");
    if (dst == NULL) {
        perror("Error opening destination file for copying");
        fclose(src);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    // Copy the source file in fixed-size chunks.
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes_read, dst);
    }

    fclose(src);
    fclose(dst);
}

// Copy a file by launching the operating system's cp command.
void copy_file_system_call(const char *src_path, const char *dst_path) {
    // Create a child process to run the system copy command.
    pid_t child_pid = fork();
    if (child_pid < 0) {
        perror("Error creating copy process");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        execlp("cp", "cp", src_path, dst_path, (char *)NULL);
        perror("Error executing cp");
        _exit(EXIT_FAILURE);
    }

    int child_status;
    if (waitpid(child_pid, &child_status, 0) < 0) {
        perror("Error waiting for copy process");
        exit(EXIT_FAILURE);
    }
    if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0) {
        fprintf(stderr, "The system-call copy failed.\n");
        exit(EXIT_FAILURE);
    }
}

double elapsed_seconds(const struct timeval *start, const struct timeval *end) {
    // Convert the time difference into seconds.
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_usec - start->tv_usec) / 1000000.0;
}

int main(void) {
    // Store user input and benchmark settings.
    char source_file[256];
    char dest_file[256];
    size_t maximum_file_size = 0;
    size_t step_size = 0;

    printf("=== COEN 146L Lab 1 - Option 2 ===\n");

    // Receive source file name from user
    printf("Enter Source File Name: ");
    if (scanf("%255s", source_file) != 1) {
        fprintf(stderr, "Invalid source file name.\n");
        return EXIT_FAILURE;
    }

    // Receive destination file name from user
    printf("Enter Destination File Name: ");
    if (scanf("%255s", dest_file) != 1) {
        fprintf(stderr, "Invalid destination file name.\n");
        return EXIT_FAILURE;
    }

    // Receive maximum file size from user
    printf("Enter Maximum File Size (in bytes): ");
    if (scanf("%zu", &maximum_file_size) != 1 || maximum_file_size == 0) {
        fprintf(stderr, "Invalid maximum file size.\n");
        return EXIT_FAILURE;
    }

    // Receive step size from user
    printf("Enter Step Size (in bytes): ");
    if (scanf("%zu", &step_size) != 1 || step_size == 0) {
        fprintf(stderr, "Invalid step size.\n");
        return EXIT_FAILURE;
    }

    // Open CSV log file for saving benchmark results
    FILE *csv_file = fopen("benchmark.csv", "w");
    if (csv_file == NULL) {
        perror("Error creating CSV log file");
        return EXIT_FAILURE;
    }
    fprintf(csv_file, "file_size_bytes,function_time_seconds,system_call_time_seconds\n");

    size_t current_size = 0;

    // Test each requested file size and compare both copy methods.
    printf("\n%-20s %-24s %-24s\n",
           "File Size (Bytes)", "Function (Seconds)", "System Call (Seconds)");
    printf("--------------------------------------------------------------------------\n");

    // Loop through sizes up to maximum_file_size
    while (current_size < maximum_file_size) {
        current_size += step_size;

        // Create/update the requested source file with current_size bytes
        generate_dummy_file(source_file, current_size);

        double function_time = 0.0;
        double system_call_time = 0.0;

        // Repeat each measurement to obtain an average time.
        for (int trial = 0; trial < NUM_TRIALS; trial++) {
            struct timeval start, end;

            gettimeofday(&start, NULL);
            copy_file(source_file, dest_file);
            gettimeofday(&end, NULL);
            function_time += elapsed_seconds(&start, &end);

            gettimeofday(&start, NULL);
            copy_file_system_call(source_file, dest_file);
            gettimeofday(&end, NULL);
            system_call_time += elapsed_seconds(&start, &end);
        }

        function_time /= NUM_TRIALS;
        system_call_time /= NUM_TRIALS;

        // Print and log measurements
        printf("%-20zu %-24.6f %-24.6f\n",
               current_size, function_time, system_call_time);
        fprintf(csv_file, "%zu,%.6f,%.6f\n",
                current_size, function_time, system_call_time);
    }

    // Close the results file before the program exits.
    fclose(csv_file);

    return EXIT_SUCCESS;
}