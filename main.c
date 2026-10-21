#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define MAX 50
#define HASH_SIZE 10

/* =====================================================
   DATA STRUCTURES
   ===================================================== */

struct Profile {
    int id;
    char name[50];
    char blood[5];
    char type[10];
    struct Profile *next;
};

struct Donation {
    int id;
    int donorProfileId;
    char name[50];
    char blood[5];
    int units;
};

struct Request {
    int id;
    char name[50];
    char blood[5];
    int units;
};

struct PriorityRequest {
    int id;
    char name[50];
    char blood[5];
    int units;
    int priority;
};

struct History {
    int id;
    int donorProfileId;
    char name[50];
    char blood[5];
    int units;
};

struct Branch {
    int id;
    char name[50];
    struct Branch *left;
    struct Branch *right;
};

struct HashNode {
    int id;
    char name[50];
    char blood[5];
    char type[10];
    struct HashNode *next;
};

/* =====================================================
   GLOBAL STORAGE
   ===================================================== */

/* Stack: donation records */
struct Donation donations[MAX];
int donationTop = -1;

/* Queue: regular blood requests */
struct Request requests[MAX];
int requestFront = 0;
int requestRear = -1;

/* Linked list: donor and receiver profiles */
struct Profile *profileHead = NULL;

/* Priority queue: urgent blood requests */
struct PriorityRequest urgentRequests[MAX];
int urgentCount = 0;

/* Circular queue: donor history */
struct History history[MAX];
int historyFront = 0;
int historyRear = -1;
int historyCount = 0;

/* BST: blood bank branches */
struct Branch *branchRoot = NULL;

/* Hash table: blood group search */
struct HashNode *hashTable[HASH_SIZE] = {NULL};

/* Automatically generated record IDs */
int nextDonationId = 1;
int nextRequestId = 1;
int nextUrgentId = 1;

/* =====================================================
   FUNCTION DECLARATIONS
   ===================================================== */

void readLine(const char *prompt, char buffer[], int size);
int readInt(const char *prompt);
void readPositiveInt(const char *prompt, int *value);
void readBloodGroup(char blood[]);
void pauseScreen(void);

struct Profile *findProfileById(int id);

void addDonation(void);
void removeDonation(void);
void displayDonations(void);

void addRequest(void);
void processRequest(void);
void displayRequests(void);

/* Functions implemented in later parts */
void addProfile(void);
void displayProfiles(void);
void deleteProfile(void);
void searchProfileById(void);

void addUrgentRequest(void);
void processUrgentRequest(void);
void displayUrgentRequests(void);

void displayHistory(void);
void removeOldestHistory(void);

void addBranch(void);
void displayBranches(void);
void searchBranch(void);

void searchBloodGroup(void);

void cleanup(void);

/* =====================================================
   INPUT UTILITIES
   ===================================================== */

void readLine(const char *prompt, char buffer[], int size) {
    int ch;
    size_t len;

    printf("%s", prompt);

    if (fgets(buffer, size, stdin) == NULL) {
        printf("\nInput ended. Exiting safely.\n");
        exit(EXIT_FAILURE);
    }

    len = strlen(buffer);

    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    } else {
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* Discard excess characters */
        }
    }
}

int readInt(const char *prompt) {
    char buffer[100];
    char *end;
    long value;

    while (1) {
        readLine(prompt, buffer, sizeof(buffer));

        errno = 0;
        value = strtol(buffer, &end, 10);

        while (*end == ' ' || *end == '\t') {
            end++;
        }

        if (end == buffer || *end != '\0' ||
            errno == ERANGE ||
            value < INT_MIN || value > INT_MAX) {
            printf("Invalid input. Enter a whole number.\n");
            continue;
        }

        return (int)value;
    }
}

void readPositiveInt(const char *prompt, int *value) {
    while (1) {
        *value = readInt(prompt);

        if (*value > 0) {
            return;
        }

        printf("Please enter a value greater than zero.\n");
    }
}

void readBloodGroup(char blood[]) {
    const char *validGroups[] = {
        "A+", "A-", "B+", "B-",
        "AB+", "AB-", "O+", "O-"
    };

    char input[30];
    int i, valid;

    while (1) {
        readLine("Blood Group (A+, A-, B+, B-, AB+, AB-, O+, O-): ",
                 input, sizeof(input));

        for (i = 0; input[i] != '\0'; i++) {
            input[i] = (char)toupper((unsigned char)input[i]);
        }

        valid = 0;

        for (i = 0; i < 8; i++) {
            if (strcmp(input, validGroups[i]) == 0) {
                valid = 1;
                break;
            }
        }

        if (valid) {
            strcpy(blood, input);
            return;
        }

        printf("Invalid blood group. Please try again.\n");
    }
}

void pauseScreen(void) {
    char buffer[10];
    readLine("\nPress Enter to continue...", buffer,
             sizeof(buffer));
}

/* =====================================================
   STACK: DONATION MANAGEMENT
   Latest donation is removed first (LIFO).
   ===================================================== */

void addDonation(void) {
    int donorId;
    struct Profile *donor;
    struct Donation entry;

    if (donationTop == MAX - 1) {
        printf("\nDonation stack is full.\n");
        return;
    }

    donorId = readInt("\nRegistered donor profile ID: ");
    donor = findProfileById(donorId);

    if (donor == NULL ||
        strcmp(donor->type, "Donor") != 0) {
        printf("\nNo registered donor found with that ID.\n");
        printf("Register the donor before recording a donation.\n");
        return;
    }

    entry.id = nextDonationId++;
    entry.donorProfileId = donor->id;

    strcpy(entry.name, donor->name);
    strcpy(entry.blood, donor->blood);

    readPositiveInt("Units donated: ", &entry.units);

    donations[++donationTop] = entry;

    printf("\nDonation recorded successfully.\n");
    printf("Donation ID: %d\n", entry.id);
    printf("Donor: %s | Blood Group: %s | Units: %d\n",
           entry.name, entry.blood, entry.units);
}

void removeDonation(void) {
    struct Donation entry;
    int i;

    if (donationTop == -1) {
        printf("\nNo donations are available.\n");
        return;
    }

    entry = donations[donationTop];

    printf("\nLatest donation removed from the stack:\n");
    printf("Donation ID: %d | Donor: %s | Blood: %s"
           " | Units: %d\n",
           entry.id, entry.name, entry.blood, entry.units);

    donationTop--;

    /*
       Removing a donation from the stack does not erase
       the historical record of the completed donation.
       History is maintained separately.
    */

    for (i = 0; i < historyCount; i++) {
        int index = (historyFront + i) % MAX;

        if (history[index].id == entry.id) {
            printf("This donation is already in the history log.\n");
            return;
        }
    }

    if (historyCount == MAX) {
        printf("History log is full. The oldest history entry "
               "must be removed before a new one can be recorded.\n");
        return;
    }

    historyRear = (historyRear + 1) % MAX;

    history[historyRear].id = entry.id;
    history[historyRear].donorProfileId = entry.donorProfileId;

    strcpy(history[historyRear].name, entry.name);
    strcpy(history[historyRear].blood, entry.blood);

    history[historyRear].units = entry.units;
    historyCount++;

    printf("Donation added to the donor history log.\n");
}

void displayDonations(void) {
    int i;

    if (donationTop == -1) {
        printf("\nNo donations have been recorded.\n");
        return;
    }

    printf("\n========== DONATION RECORDS ==========\n");

    for (i = donationTop; i >= 0; i--) {
        printf("Donation ID: %d | Donor ID: %d\n",
               donations[i].id,
               donations[i].donorProfileId);

        printf("Donor: %s | Blood Group: %s | Units: %d\n",
               donations[i].name,
               donations[i].blood,
               donations[i].units);

        printf("--------------------------------------\n");
    }
}

/* =====================================================
   QUEUE: REGULAR BLOOD REQUESTS
   Oldest request is processed first (FIFO).
   ===================================================== */

void addRequest(void) {
    struct Request entry;

    if (requestRear == MAX - 1) {
        printf("\nRequest queue is full.\n");
        return;
    }

    entry.id = nextRequestId++;

    readLine("\nPatient Name: ", entry.name,
             sizeof(entry.name));

    if (entry.name[0] == '\0') {
        printf("Patient name cannot be empty.\n");
        nextRequestId--;
        return;
    }

    readBloodGroup(entry.blood);
    readPositiveInt("Units required: ", &entry.units);

    requests[++requestRear] = entry;

    printf("\nBlood request registered successfully.\n");
    printf("Request ID: %d | Patient: %s\n",
           entry.id, entry.name);
}

void processRequest(void) {
    int i;
    struct Request entry;

    if (requestFront > requestRear) {
        printf("\nThere are no pending regular requests.\n");
        return;
    }

    entry = requests[requestFront];

    printf("\nProcessing oldest blood request:\n");
    printf("Request ID: %d | Patient: %s\n",
           entry.id, entry.name);

    printf("Blood Group: %s | Units: %d\n",
           entry.blood, entry.units);

    for (i = requestFront; i < requestRear; i++) {
        requests[i] = requests[i + 1];
    }

    requestRear--;

    if (requestRear < requestFront) {
        requestFront = 0;
        requestRear = -1;
    }

    printf("Request processed from the queue.\n");
}

void displayRequests(void) {
    int i;

    if (requestFront > requestRear) {
        printf("\nThere are no pending regular requests.\n");
        return;
    }

    printf("\n========== PENDING BLOOD REQUESTS ==========\n");

    for (i = requestFront; i <= requestRear; i++) {
        printf("Request ID: %d | Patient: %s\n",
               requests[i].id, requests[i].name);

        printf("Blood Group: %s | Units: %d\n",
               requests[i].blood, requests[i].units);

        printf("--------------------------------------------\n");
    }
}


/* =====================================================
   LINKED LIST: DONOR AND RECEIVER PROFILES
   ===================================================== */

struct Profile *findProfileById(int id) {
    struct Profile *current = profileHead;

    while (current != NULL) {
        if (current->id == id) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

void addProfile(void) {
    struct Profile *newNode;
    int id;

    id = readInt("\nEnter unique profile ID: ");

    if (findProfileById(id) != NULL) {
        printf("\nA profile with this ID already exists.\n");
        return;
    }

    newNode = malloc(sizeof(struct Profile));

    if (newNode == NULL) {
        printf("\nUnable to allocate memory for profile.\n");
        return;
    }

    newNode->id = id;

    readLine("Full Name: ", newNode->name,
             sizeof(newNode->name));

    if (newNode->name[0] == '\0') {
        printf("Name cannot be empty.\n");
        free(newNode);
        return;
    }

    readBloodGroup(newNode->blood);

    while (1) {
        readLine("Profile Type (Donor/Receiver): ",
                 newNode->type, sizeof(newNode->type));

        if (strcmp(newNode->type, "Donor") == 0 ||
            strcmp(newNode->type, "Receiver") == 0) {
            break;
        }

        if (strcmp(newNode->type, "donor") == 0) {
            strcpy(newNode->type, "Donor");
            break;
        }

        if (strcmp(newNode->type, "receiver") == 0) {
            strcpy(newNode->type, "Receiver");
            break;
        }

        printf("Please enter Donor or Receiver.\n");
    }

    newNode->next = NULL;

    if (profileHead == NULL) {
        profileHead = newNode;
    } else {
        struct Profile *current = profileHead;

        while (current->next != NULL) {
            current = current->next;
        }

        current->next = newNode;
    }

    printf("\nProfile registered successfully!\n");
    printf("Profile ID: %d\n", newNode->id);
    printf("Name: %s | Blood Group: %s | Type: %s\n",
           newNode->name, newNode->blood, newNode->type);
}

void displayProfiles(void) {
    struct Profile *current = profileHead;

    if (current == NULL) {
        printf("\nNo donor or receiver profiles registered yet.\n");
        return;
    }

    printf("\n========== REGISTERED PROFILES ==========\n");

    while (current != NULL) {
        printf("Profile ID: %d\n", current->id);
        printf("Name: %s\n", current->name);
        printf("Blood Group: %s\n", current->blood);
        printf("Profile Type: %s\n", current->type);
        printf("-----------------------------------------\n");

        current = current->next;
    }
}

void searchProfileById(void) {
    int id;
    struct Profile *profile;

    id = readInt("\nEnter profile ID: ");
    profile = findProfileById(id);

    if (profile == NULL) {
        printf("\nNo profile found with ID %d.\n", id);
        return;
    }

    printf("\n========== PROFILE DETAILS ==========\n");
    printf("Profile ID: %d\n", profile->id);
    printf("Name: %s\n", profile->name);
    printf("Blood Group: %s\n", profile->blood);
    printf("Type: %s\n", profile->type);
}

void deleteProfile(void) {
    int id;
    struct Profile *current = profileHead;
    struct Profile *previous = NULL;

    id = readInt("\nEnter profile ID to delete: ");

    while (current != NULL && current->id != id) {
        previous = current;
        current = current->next;
    }

    if (current == NULL) {
        printf("\nProfile not found.\n");
        return;
    }

    /*
       Preserve referential integrity: do not delete a profile
       while active donation or request records refer to it.
    */

    if (donationTop >= 0) {
        int i;

        for (i = 0; i <= donationTop; i++) {
            if (donations[i].donorProfileId == id) {
                printf("\nCannot delete this profile while "
                       "its donation is in the active stack.\n");
                return;
            }
        }
    }

    if (historyCount > 0) {
        int i;

        for (i = 0; i < historyCount; i++) {
            int index = (historyFront + i) % MAX;

            if (history[index].donorProfileId == id) {
                printf("\nCannot delete this profile because "
                       "its donation history is retained.\n");
                return;
            }
        }
    }

    if (previous == NULL) {
        profileHead = current->next;
    } else {
        previous->next = current->next;
    }

    free(current);

    printf("\nProfile deleted successfully.\n");
}

/* =====================================================
   PRIORITY QUEUE: URGENT BLOOD REQUESTS
   Priority 3 = High, 2 = Medium, 1 = Low.
   ===================================================== */

void addUrgentRequest(void) {
    struct PriorityRequest entry;
    int i;

    if (urgentCount == MAX) {
        printf("\nUrgent request queue is full.\n");
        return;
    }

    entry.id = nextUrgentId++;

    readLine("\nPatient Name: ", entry.name,
             sizeof(entry.name));

    if (entry.name[0] == '\0') {
        printf("Patient name cannot be empty.\n");
        nextUrgentId--;
        return;
    }

    readBloodGroup(entry.blood);
    readPositiveInt("Units required: ", &entry.units);

    while (1) {
        entry.priority = readInt(
            "Urgency (1-Low, 2-Medium, 3-High): ");

        if (entry.priority >= 1 && entry.priority <= 3) {
            break;
        }

        printf("Enter 1, 2, or 3.\n");
    }

    /*
       Insert in descending priority order.
       Equal-priority requests keep their arrival order.
    */

    i = urgentCount;

    while (i > 0 &&
           urgentRequests[i - 1].priority < entry.priority) {
        urgentRequests[i] = urgentRequests[i - 1];
        i--;
    }

    urgentRequests[i] = entry;
    urgentCount++;

    printf("\nUrgent blood request registered successfully!\n");
    printf("Request ID: %d | Patient: %s\n",
           entry.id, entry.name);
    printf("Blood Group: %s | Units: %d | Priority: %d\n",
           entry.blood, entry.units, entry.priority);
}

void processUrgentRequest(void) {
    int i;
    struct PriorityRequest entry;

    if (urgentCount == 0) {
        printf("\nNo urgent blood requests are pending.\n");
        return;
    }

    entry = urgentRequests[0];

    printf("\n========== PROCESSING URGENT REQUEST ==========\n");
    printf("Request ID: %d\n", entry.id);
    printf("Patient: %s\n", entry.name);
    printf("Blood Group: %s | Units: %d\n",
           entry.blood, entry.units);
    printf("Priority: %d\n", entry.priority);

    for (i = 0; i < urgentCount - 1; i++) {
        urgentRequests[i] = urgentRequests[i + 1];
    }

    urgentCount--;

    printf("Urgent request processed from the queue.\n");
}

void displayUrgentRequests(void) {
    int i;

    if (urgentCount == 0) {
        printf("\nNo urgent blood requests are pending.\n");
        return;
    }

    printf("\n========== URGENT BLOOD REQUESTS ==========\n");

    for (i = 0; i < urgentCount; i++) {
        printf("Request ID: %d | Patient: %s\n",
               urgentRequests[i].id,
               urgentRequests[i].name);

        printf("Blood Group: %s | Units: %d | Priority: %d\n",
               urgentRequests[i].blood,
               urgentRequests[i].units,
               urgentRequests[i].priority);

        printf("--------------------------------------------\n");
    }
}


/* =====================================================
   CIRCULAR QUEUE: DONOR HISTORY
   ===================================================== */

void displayHistory(void) {
    int i;
    int index;

    if (historyCount == 0) {
        printf("\nNo donation history available.\n");
        return;
    }

    printf("\n========== DONOR HISTORY LOG ==========\n");

    index = historyFront;

    for (i = 0; i < historyCount; i++) {
        printf("Donation ID: %d | Donor ID: %d\n",
               history[index].id,
               history[index].donorProfileId);

        printf("Donor: %s | Blood Group: %s | Units: %d\n",
               history[index].name,
               history[index].blood,
               history[index].units);

        printf("---------------------------------------\n");

        index = (index + 1) % MAX;
    }
}

void removeOldestHistory(void) {
    if (historyCount == 0) {
        printf("\nNo donation history to remove.\n");
        return;
    }

    printf("\nRemoving oldest history entry:\n");
    printf("Donation ID: %d | Donor: %s | Blood: %s\n",
           history[historyFront].id,
           history[historyFront].name,
           history[historyFront].blood);

    historyFront = (historyFront + 1) % MAX;
    historyCount--;

    if (historyCount == 0) {
        historyFront = 0;
        historyRear = -1;
    }

    printf("Oldest history entry removed successfully.\n");
}

/* =====================================================
   BINARY SEARCH TREE: BLOOD BANK BRANCHES
   ===================================================== */

struct Branch *insertBranchNode(struct Branch *root,
                                int id, const char name[],
                                int *inserted) {
    struct Branch *newNode;

    if (root == NULL) {
        newNode = malloc(sizeof(struct Branch));

        if (newNode == NULL) {
            printf("\nMemory allocation failed.\n");
            return NULL;
        }

        newNode->id = id;

        snprintf(newNode->name, sizeof(newNode->name),
                 "%s", name);

        newNode->left = NULL;
        newNode->right = NULL;

        *inserted = 1;
        return newNode;
    }

    if (id < root->id) {
        struct Branch *child =
            insertBranchNode(root->left, id, name, inserted);

        if (*inserted) {
            root->left = child;
        }
    } else if (id > root->id) {
        struct Branch *child =
            insertBranchNode(root->right, id, name, inserted);

        if (*inserted) {
            root->right = child;
        }
    } else {
        printf("\nA branch with this ID already exists.\n");
    }

    return root;
}

struct Branch *findBranchNode(struct Branch *root, int id) {
    if (root == NULL || root->id == id) {
        return root;
    }

    if (id < root->id) {
        return findBranchNode(root->left, id);
    }

    return findBranchNode(root->right, id);
}

void addBranch(void) {
    int id;
    int inserted = 0;
    char name[50];

    id = readInt("\nBranch ID: ");

    if (findBranchNode(branchRoot, id) != NULL) {
        printf("\nA branch with this ID already exists.\n");
        return;
    }

    readLine("Branch Name: ", name, sizeof(name));

    if (name[0] == '\0') {
        printf("Branch name cannot be empty.\n");
        return;
    }

    branchRoot = insertBranchNode(branchRoot, id,
                                  name, &inserted);

    if (inserted) {
        printf("\nBlood bank branch added successfully.\n");
    }
}

void displayBranches(void) {
    struct Branch *stack[MAX];
    struct Branch *current = branchRoot;
    int topIndex = -1;

    if (branchRoot == NULL) {
        printf("\nNo blood bank branches registered.\n");
        return;
    }

    printf("\n========== BLOOD BANK BRANCHES ==========\n");

    /*
       Iterative inorder traversal displays branches
       in ascending branch-ID order.
    */

    while (current != NULL || topIndex >= 0) {
        while (current != NULL) {
            if (topIndex == MAX - 1) {
                printf("Tree traversal stack capacity exceeded.\n");
                return;
            }

            stack[++topIndex] = current;
            current = current->left;
        }

        current = stack[topIndex--];

        printf("Branch ID: %d | Branch Name: %s\n",
               current->id, current->name);

        current = current->right;
    }
}

void searchBranch(void) {
    int id;
    struct Branch *result;

    if (branchRoot == NULL) {
        printf("\nNo branches registered yet.\n");
        return;
    }

    id = readInt("\nEnter branch ID to search: ");
    result = findBranchNode(branchRoot, id);

    if (result == NULL) {
        printf("\nNo branch found with ID %d.\n", id);
        return;
    }

    printf("\n========== BRANCH FOUND ==========\n");
    printf("Branch ID: %d\n", result->id);
    printf("Branch Name: %s\n", result->name);
}

/* =====================================================
   HASH TABLE: SEARCH PROFILES BY BLOOD GROUP
   Collision handling: separate chaining.
   ===================================================== */

int hashFunction(const char blood[]) {
    unsigned int sum = 0;
    int i;

    for (i = 0; blood[i] != '\0'; i++) {
        sum += (unsigned char)blood[i];
    }

    return (int)(sum % HASH_SIZE);
}

void clearHashTable(void) {
    int i;
    struct HashNode *current;
    struct HashNode *next;

    for (i = 0; i < HASH_SIZE; i++) {
        current = hashTable[i];

        while (current != NULL) {
            next = current->next;
            free(current);
            current = next;
        }

        hashTable[i] = NULL;
    }
}

int buildHashTable(void) {
    struct Profile *profile;
    struct HashNode *newTable[HASH_SIZE] = {NULL};
    struct HashNode *tails[HASH_SIZE] = {NULL};
    struct HashNode *node;
    struct HashNode *current;
    int i;
    int index;

    /*
       Construct a fresh table separately.
       If allocation fails, retain the existing table.
    */

    profile = profileHead;

    while (profile != NULL) {
        index = hashFunction(profile->blood);

        node = malloc(sizeof(struct HashNode));

        if (node == NULL) {
            printf("\nMemory allocation failed. "
                   "The existing search table was preserved.\n");

            for (i = 0; i < HASH_SIZE; i++) {
                current = newTable[i];

                while (current != NULL) {
                    struct HashNode *next = current->next;
                    free(current);
                    current = next;
                }
            }

            return 0;
        }

        node->id = profile->id;

        snprintf(node->name, sizeof(node->name),
                 "%s", profile->name);

        snprintf(node->blood, sizeof(node->blood),
                 "%s", profile->blood);

        snprintf(node->type, sizeof(node->type),
                 "%s", profile->type);

        node->next = NULL;

        if (newTable[index] == NULL) {
            newTable[index] = node;
            tails[index] = node;
        } else {
            tails[index]->next = node;
            tails[index] = node;
        }

        profile = profile->next;
    }

    clearHashTable();

    for (i = 0; i < HASH_SIZE; i++) {
        hashTable[i] = newTable[i];
    }

    return 1;
}

void searchBloodGroup(void) {
    char blood[5];
    struct HashNode *current;
    int index;
    int found = 0;

    if (profileHead == NULL) {
        printf("\nNo profiles registered yet.\n");
        printf("Register donors or receivers before searching.\n");
        return;
    }

    /*
       Rebuild before each search so additions and deletions
       in the linked list are reflected in the search results.
    */

    if (!buildHashTable()) {
        return;
    }

    readBloodGroup(blood);

    index = hashFunction(blood);
    current = hashTable[index];

    printf("\n========== BLOOD GROUP SEARCH ==========\n");
    printf("Searching for blood group: %s\n\n", blood);

    while (current != NULL) {
        if (strcmp(current->blood, blood) == 0) {
            printf("Profile ID: %d\n", current->id);
            printf("Name: %s\n", current->name);
            printf("Blood Group: %s\n", current->blood);
            printf("Profile Type: %s\n", current->type);
            printf("----------------------------------------\n");

            found = 1;
        }

        current = current->next;
    }

    if (!found) {
        printf("No profiles found for blood group %s.\n", blood);
    }
}

/* =====================================================
   CLEANUP: RELEASE DYNAMICALLY ALLOCATED MEMORY
   ===================================================== */

void freeBranches(struct Branch *root) {
    if (root == NULL) {
        return;
    }

    freeBranches(root->left);
    freeBranches(root->right);
    free(root);
}

void cleanup(void) {
    struct Profile *current;
    struct Profile *next;

    /* Free linked-list profiles */
    current = profileHead;

    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }

    profileHead = NULL;

    /* Free all BST branch nodes */
    freeBranches(branchRoot);
    branchRoot = NULL;

    /* Free all hash-table nodes */
    clearHashTable();
}

/* =====================================================
   PORTAL MENU: DONATION MANAGEMENT
   ===================================================== */

void donationMenu(void) {
    int choice;

    do {
        printf("\n========== DONATION MANAGEMENT ==========\n");
        printf("1. Record Donation\n");
        printf("2. Remove Latest Donation from Stack\n");
        printf("3. View Donation Records\n");
        printf("4. Return to Main Menu\n");

        choice = readInt("\nEnter choice: ");

        switch (choice) {
            case 1:
                addDonation();
                break;

            case 2:
                removeDonation();
                break;

            case 3:
                displayDonations();
                break;

            case 4:
                break;

            default:
                printf("\nInvalid choice. Try again.\n");
        }

        if (choice != 4) {
            pauseScreen();
        }

    } while (choice != 4);
}

/* =====================================================
   PORTAL MENU: BLOOD REQUEST MANAGEMENT
   ===================================================== */

void requestMenu(void) {
    int choice;

    do {
        printf("\n========== BLOOD REQUEST MANAGEMENT ==========\n");
        printf("1. Register Regular Blood Request\n");
        printf("2. Process Oldest Regular Request\n");
        printf("3. View Pending Regular Requests\n");
        printf("4. Return to Main Menu\n");

        choice = readInt("\nEnter choice: ");

        switch (choice) {
            case 1:
                addRequest();
                break;

            case 2:
                processRequest();
                break;

            case 3:
                displayRequests();
                break;

            case 4:
                break;

            default:
                printf("\nInvalid choice. Try again.\n");
        }

        if (choice != 4) {
            pauseScreen();
        }

    } while (choice != 4);
}

/* =====================================================
   PORTAL MENU: PROFILE MANAGEMENT
   ===================================================== */

void profileMenu(void) {
    int choice;

    do {
        printf("\n========== DONOR & RECEIVER PROFILES ==========\n");
        printf("1. Register New Profile\n");
        printf("2. View All Profiles\n");
        printf("3. Search Profile by ID\n");
        printf("4. Delete Profile\n");
        printf("5. Return to Main Menu\n");

        choice = readInt("\nEnter choice: ");

        switch (choice) {
            case 1:
                addProfile();
                break;

            case 2:
                displayProfiles();
                break;

            case 3:
                searchProfileById();
                break;

            case 4:
                deleteProfile();
                break;

            case 5:
                break;

            default:
                printf("\nInvalid choice. Try again.\n");
        }

        if (choice != 5) {
            pauseScreen();
        }

    } while (choice != 5);
}

/* =====================================================
   PORTAL MENU: URGENT BLOOD REQUESTS
   ===================================================== */

void urgentRequestMenu(void) {
    int choice;

    do {
        printf("\n========== URGENT BLOOD REQUESTS ==========\n");
        printf("1. Register Urgent Request\n");
        printf("2. Process Highest-Priority Request\n");
        printf("3. View Urgent Requests\n");
        printf("4. Return to Main Menu\n");

        choice = readInt("\nEnter choice: ");

        switch (choice) {
            case 1:
                addUrgentRequest();
                break;

            case 2:
                processUrgentRequest();
                break;

            case 3:
                displayUrgentRequests();
                break;

            case 4:
                break;

            default:
                printf("\nInvalid choice. Try again.\n");
        }

        if (choice != 4) {
            pauseScreen();
        }

    } while (choice != 4);
}

/* =====================================================
   PORTAL MENU: DONOR HISTORY
   ===================================================== */

void historyMenu(void) {
    int choice;

    do {
        printf("\n========== DONOR HISTORY LOG ==========\n");
        printf("1. View Donation History\n");
        printf("2. Remove Oldest History Entry\n");
        printf("3. Return to Main Menu\n");

        choice = readInt("\nEnter choice: ");

        switch (choice) {
            case 1:
                displayHistory();
                break;

            case 2:
                removeOldestHistory();
                break;

            case 3:
                break;

            default:
                printf("\nInvalid choice. Try again.\n");
        }

        if (choice != 3) {
            pauseScreen();
        }

    } while (choice != 3);
}

/* =====================================================
   PORTAL MENU: BLOOD BANK BRANCHES
   ===================================================== */

void branchMenu(void) {
    int choice;

    do {
        printf("\n========== BLOOD BANK BRANCHES ==========\n");
        printf("1. Register Branch\n");
        printf("2. View All Branches\n");
        printf("3. Search Branch by ID\n");
        printf("4. Return to Main Menu\n");

        choice = readInt("\nEnter choice: ");

        switch (choice) {
            case 1:
                addBranch();
                break;

            case 2:
                displayBranches();
                break;

            case 3:
                searchBranch();
                break;

            case 4:
                break;

            default:
                printf("\nInvalid choice. Try again.\n");
        }

        if (choice != 4) {
            pauseScreen();
        }

    } while (choice != 4);
}

/* =====================================================
   MAIN MENU: BLOOD DONATION & REQUEST PORTAL
   ===================================================== */

int main(void) {
    int choice;

    do {
        printf("\n");
        printf("============================================\n");
        printf("       BLOOD DONATION & REQUEST PORTAL      \n");
        printf("============================================\n");
        printf(" Supporting SDG 3: Good Health and Well-being\n");
        printf("--------------------------------------------\n");
        printf(" 1. Donation Management\n");
        printf(" 2. Blood Request Management\n");
        printf(" 3. Donor & Receiver Profiles\n");
        printf(" 4. Urgent Blood Requests\n");
        printf(" 5. Donor History Log\n");
        printf(" 6. Blood Bank Branches\n");
        printf(" 7. Search Profiles by Blood Group\n");
        printf(" 8. Exit Portal\n");
        printf("--------------------------------------------\n");

        choice = readInt("Enter your choice: ");

        switch (choice) {
            case 1:
                donationMenu();
                break;

            case 2:
                requestMenu();
                break;

            case 3:
                profileMenu();
                break;

            case 4:
                urgentRequestMenu();
                break;

            case 5:
                historyMenu();
                break;

            case 6:
                branchMenu();
                break;

            case 7:
                searchBloodGroup();
                pauseScreen();
                break;

            case 8:
                printf("\nThank you for using the portal.\n");
                printf("Donate blood. Support life.\n");
                cleanup();
                break;

            default:
                printf("\nInvalid choice. Please select 1-8.\n");
                pauseScreen();
        }

    } while (choice != 8);

    return 0;
}