#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main() {
    key_t key = ftok("/", 'A');  
    if (key == -1) {
        perror("ftok failed");
        return EXIT_FAILURE;
    }

    int shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        perror("shmget failed");
        return EXIT_FAILURE;
    }

    // Remove the shared memory segment
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl failed");
        return EXIT_FAILURE;
    }

    printf("Shared memory removed successfully.\n");
    return EXIT_SUCCESS;
}
