include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define MAX 50
#define HASH_SIZE 10

struct Profile {
	int id;
	char name[50];
	char blood[5];
	char type[10];
	struct Profile *next;   /* next profile in the main linked list */
	struct Profile *hnext;  /* next profile in the same hash bucket */
};

/* Used for both the donation stack and the history log */
struct Donation {
	int id;
	int donorProfileId;
	char name[50];
	char blood[5];
	int units;
};

/* Used for both regular and urgent requests (priority is 0 for regular ones) */
struct Request {
	int id;
	char name[50];
	char blood[5];
	int units;
	int priority;
};

struct Branch {
	int id;
	char name[50];
	struct Branch *left;
	struct Branch *right;
};

/* ---------- global data ---------- */

struct Donation donations[MAX];          /* stack */
int donationTop = -1;

struct Request requests[MAX];            /* FIFO queue */
int requestCount = 0;

struct Profile *profileHead = NULL;      /* linked list */
struct Profile *hashTable[HASH_SIZE];    /* hash table with chaining */

struct Request urgentRequests[MAX];      /* priority queue (sorted array) */
int urgentCount = 0;

struct Donation history[MAX];            /* circular queue */
int historyFront = 0;
int historyCount = 0;

struct Branch *branchRoot = NULL;        /* binary search tree */

int nextDonationId = 1;
int nextRequestId = 1;
int nextUrgentId = 1;

/* ---------- input helpers ---------- */

/* Reads one line safely and removes the newline.
   If the line is too long, the rest of it is discarded. */
void readLine(const char *prompt, char buffer[], int size)
{
	int ch;
	size_t length;

	printf("%s", prompt);
	if (fgets(buffer, size, stdin) == NULL) {
		printf("\nInput ended. Exiting safely.\n");
		exit(EXIT_FAILURE);
	}

	length = strlen(buffer);
	if (length > 0 && buffer[length - 1] == '\n') {
		buffer[length - 1] = '\0';
	} else {
		ch = getchar();
		while (ch != '\n' && ch != EOF)
			ch = getchar();
	}
}

/* Keeps asking until the user types a valid whole number in int range. */
int readInt(const char *prompt)
{
	char buffer[100];
	char *end;
	long value;

	while (1) {
		readLine(prompt, buffer, sizeof(buffer));

		errno = 0;
		value = strtol(buffer, &end, 10);

		/* allow trailing spaces or tabs after the number */
		while (*end == ' ' || *end == '\t')
			end++;

		if (end == buffer || *end != '\0' || errno == ERANGE ||
		        value < INT_MIN || value > INT_MAX) {
			printf("Invalid input. Enter a whole number.\n");
		} else {
			return (int)value;
		}
	}
}

int readPositiveInt(const char *prompt)
{
	int value = readInt(prompt);

	while (value <= 0) {
		printf("Please enter a value greater than zero.\n");
		value = readInt(prompt);
	}
	return value;
}

/* Accepts only the eight valid blood groups (lowercase is converted). */
void readBloodGroup(char blood[])
{
	const char *groups[8] = {"A+", "A-", "B+", "B-", "AB+", "AB-", "O+", "O-"};
	char input[30];
	int i;

	while (1) {
		readLine("Blood Group (A+, A-, B+, B-, AB+, AB-, O+, O-): ", input, sizeof(input));

		for (i = 0; input[i] != '\0'; i++)
			input[i] = (char)toupper((unsigned char)input[i]);

		for (i = 0; i < 8; i++) {
			if (strcmp(input, groups[i]) == 0) {
				strcpy(blood, input);
				return;
			}
		}
		printf("Invalid blood group. Please try again.\n");
	}
}

/* Reads a name. Returns 1 if it is not empty; otherwise prints the error and returns 0. */
int readName(const char *prompt, char buffer[], int size, const char *errorMessage)
{
	readLine(prompt, buffer, size);
	if (buffer[0] != '\0')
		return 1;

	printf("%s\n", errorMessage);
	return 0;
}

void pauseScreen(void)
{
	char buffer[10];
	readLine("\nPress Enter to continue...", buffer, sizeof(buffer));
}

/* ---------- STACK: donation records (Last In, First Out) ---------- */

/* Linear search through the profile linked list. */
struct Profile *findProfileById(int id)
{
	struct Profile *current = profileHead;

	while (current != NULL) {
		if (current->id == id)
			return current;
		current = current->next;
	}
	return NULL;
}

void showDonation(const struct Donation *d, const char *separator)
{
	printf("Donation ID: %d | Donor ID: %d\n", d->id, d->donorProfileId);
	printf("Donor: %s | Blood Group: %s | Units: %d\n", d->name, d->blood, d->units);
	printf("%s\n", separator);
}

/* PUSH */
void addDonation(void)
{
	struct Donation entry;
	struct Profile *donor;
	int donorId;

	if (donationTop == MAX - 1) {
		printf("\nDonation stack is full.\n");
		return;
	}

	donorId = readInt("\nRegistered donor profile ID: ");
	donor = findProfileById(donorId);
	if (donor == NULL || strcmp(donor->type, "Donor") != 0) {
		printf("\n No registered donor found with that ID.\n");
		printf("Register the donor before recording a donation.\n");
		return;
	}

	entry.id = nextDonationId;
	nextDonationId++;
	entry.donorProfileId = donor->id;
	strcpy(entry.name, donor->name);
	strcpy(entry.blood, donor->blood);
	entry.units = readPositiveInt("Units donated: ");

	donationTop++;
	donations[donationTop] = entry;

	printf("\nDonation recorded successfully.\n");
	printf("Donation ID: %d\n", entry.id);
	printf("Donor: %s | Blood Group: %s | Units: %d\n", entry.name, entry.blood, entry.units);
}

/* POP, then move the donation into the history circular queue. */
void removeDonation(void)
{
	struct Donation entry;
	int rear;

	if (donationTop == -1) {
		printf("\nNo donations are available.\n");
		return;
	}

	/* Check history space BEFORE popping, so the donation is never lost. */
	if (historyCount == MAX) {
		printf("\nHistory log is full. The oldest history entry "
		       "must be removed before a new one can be recorded.\n");
		return;
	}

	entry = donations[donationTop];
	donationTop--;

	printf("\nLatest donation removed from the stack:\n");
	printf("Donation ID: %d | Donor: %s | Blood: %s | Units: %d\n",
	       entry.id, entry.name, entry.blood, entry.units);

	/* enqueue at the rear of the circular queue */
	rear = (historyFront + historyCount) % MAX;
	history[rear] = entry;
	historyCount++;
	printf("Donation added to the donor history log.\n");
}

/* Display from top of stack (newest) to bottom (oldest). */
void displayDonations(void)
{
	int i;

	if (donationTop == -1) {
		printf("\nNo donations have been recorded.\n");
		return;
	}

	printf("\n========== DONATION RECORDS ==========\n");
	for (i = donationTop; i >= 0; i--)
		showDonation(&donations[i], "--------------------------------------");
}

/* ---------- QUEUE: regular blood requests (First In, First Out) ---------- */

/* Reads patient details for a regular or urgent request.
   The ID is assigned only after the name is accepted,
   so a cancelled registration does not use up an ID.
   Returns 1 on success and 0 if cancelled. */
int readRequest(struct Request *request, int *nextId)
{
	if (!readName("\nPatient Name: ", request->name, sizeof(request->name),
	              "Patient name cannot be empty."))
		return 0;

	request->id = *nextId;
	(*nextId)++;
	readBloodGroup(request->blood);
	request->units = readPositiveInt("Units required: ");
	request->priority = 0;
	return 1;
}

/* Removes and returns the first element, shifting the rest one place left.
   Used by both the regular queue and the priority queue. */
struct Request popFront(struct Request queue[], int *count)
{
	struct Request first = queue[0];
	int i;

	for (i = 0; i < *count - 1; i++)
		queue[i] = queue[i + 1];
	(*count)--;

	return first;
}

/* ENQUEUE at the rear */
void addRequest(void)
{
	struct Request request;

	if (requestCount == MAX) {
		printf("\nRequest queue is full.\n");
		return;
	}

	if (!readRequest(&request, &nextRequestId))
		return;

	requests[requestCount] = request;
	requestCount++;

	printf("\nBlood request registered successfully.\n");
	printf("Request ID: %d | Patient: %s\n", request.id, request.name);
}

/* DEQUEUE from the front (oldest request) */
void processRequest(void)
{
	struct Request request;

	if (requestCount == 0) {
		printf("\nThere are no pending regular requests.\n");
		return;
	}

	request = popFront(requests, &requestCount);

	printf("\nProcessing oldest blood request:\n");
	printf("Request ID: %d | Patient: %s\n", request.id, request.name);
	printf("Blood Group: %s | Units: %d\n", request.blood, request.units);
	printf("Request processed from the queue.\n");
}

void displayRequests(void)
{
	int i;

	if (requestCount == 0) {
		printf("\nThere are no pending regular requests.\n");
		return;
	}

	printf("\n========== PENDING BLOOD REQUESTS ==========\n");
	for (i = 0; i < requestCount; i++) {
		printf("Request ID: %d | Patient: %s\n", requests[i].id, requests[i].name);
		printf("Blood Group: %s | Units: %d\n", requests[i].blood, requests[i].units);
		printf("--------------------------------------------\n");
	}
}

/* ---------- LINKED LIST + HASH TABLE: profiles ----------
   Every profile node is in TWO chains at once:
     - the main linked list (through 'next')
     - one hash bucket chain (through 'hnext')
   Both chains point to the same node, so it is allocated and freed once. */

/* Hash = sum of the characters' ASCII codes, modulo HASH_SIZE.
   Different blood groups can land in the same bucket (collision),
   so searches still compare the full blood group string. */
int hashFunction(const char blood[])
{
	int sum = 0;
	int i;

	for (i = 0; blood[i] != '\0'; i++)
		sum += (unsigned char)blood[i];

	return sum % HASH_SIZE;
}

void showProfile(const struct Profile *p, const char *typeLabel, const char *separator)
{
	printf("Profile ID: %d\n", p->id);
	printf("Name: %s\n", p->name);
	printf("Blood Group: %s\n", p->blood);
	printf("%s: %s\n", typeLabel, p->type);
	if (separator != NULL)
		printf("%s\n", separator);
}

void addProfile(void)
{
	struct Profile *newProfile;
	struct Profile *current;
	int index;
	int id = readInt("\nEnter unique profile ID: ");

	if (findProfileById(id) != NULL) {
		printf("\nA profile with this ID already exists.\n");
		return;
	}

	newProfile = malloc(sizeof(struct Profile));
	if (newProfile == NULL) {
		printf("\nUnable to allocate memory for profile.\n");
		return;
	}
	newProfile->id = id;
	newProfile->next = NULL;
	newProfile->hnext = NULL;

	if (!readName("Full Name: ", newProfile->name, sizeof(newProfile->name),
	              "Name cannot be empty.")) {
		free(newProfile);
		return;
	}

	readBloodGroup(newProfile->blood);

	while (1) {
		readLine("Profile Type (Donor/Receiver): ", newProfile->type, sizeof(newProfile->type));

		if (strcmp(newProfile->type, "donor") == 0)
			strcpy(newProfile->type, "Donor");
		else if (strcmp(newProfile->type, "receiver") == 0)
			strcpy(newProfile->type, "Receiver");

		if (strcmp(newProfile->type, "Donor") == 0 || strcmp(newProfile->type, "Receiver") == 0)
			break;
		printf("Please enter Donor or Receiver.\n");
	}

	/* Insert at the END of the main linked list (keeps registration order). */
	if (profileHead == NULL) {
		profileHead = newProfile;
	} else {
		current = profileHead;
		while (current->next != NULL)
			current = current->next;
		current->next = newProfile;
	}

	/* Insert at the END of its hash bucket chain. */
	index = hashFunction(newProfile->blood);
	if (hashTable[index] == NULL) {
		hashTable[index] = newProfile;
	} else {
		current = hashTable[index];
		while (current->hnext != NULL)
			current = current->hnext;
		current->hnext = newProfile;
	}

	printf("\nProfile registered successfully!\n");
	printf("Profile ID: %d\n", newProfile->id);
	printf("Name: %s | Blood Group: %s | Type: %s\n",
	       newProfile->name, newProfile->blood, newProfile->type);
}

void displayProfiles(void)
{
	struct Profile *current = profileHead;

	if (current == NULL) {
		printf("\nNo donor or receiver profiles registered yet.\n");
		return;
	}

	printf("\n========== REGISTERED PROFILES ==========\n");
	while (current != NULL) {
		showProfile(current, "Profile Type", "-----------------------------------------");
		current = current->next;
	}
}

void searchProfileById(void)
{
	int id = readInt("\nEnter profile ID: ");
	struct Profile *profile = findProfileById(id);

	if (profile == NULL) {
		printf("\nNo profile found with ID %d.\n", id);
		return;
	}

	printf("\n========== PROFILE DETAILS ==========\n");
	showProfile(profile, "Type", NULL);
}

void deleteProfile(void)
{
	struct Profile *current = profileHead;
	struct Profile *previous = NULL;
	struct Profile *bucketCurrent;
	struct Profile *bucketPrevious = NULL;
	int i, index, position;
	int id = readInt("\nEnter profile ID to delete: ");

	/* Find the node and the node before it in the main list. */
	while (current != NULL && current->id != id) {
		previous = current;
		current = current->next;
	}
	if (current == NULL) {
		printf("\nProfile not found.\n");
		return;
	}

	/* Do not delete a profile that donation records still refer to. */
	for (i = 0; i <= donationTop; i++) {
		if (donations[i].donorProfileId == id) {
			printf("\nCannot delete this profile while its donation is in the active stack.\n");
			return;
		}
	}
	for (i = 0; i < historyCount; i++) {
		position = (historyFront + i) % MAX;
		if (history[position].donorProfileId == id) {
			printf("\nCannot delete this profile because its donation history is retained.\n");
			return;
		}
	}

	/* Unlink from the main linked list. */
	if (previous == NULL)
		profileHead = current->next;      /* deleting the first node */
	else
		previous->next = current->next;

	/* Unlink the same node from its hash bucket. */
	index = hashFunction(current->blood);
	bucketCurrent = hashTable[index];
	while (bucketCurrent != NULL && bucketCurrent != current) {
		bucketPrevious = bucketCurrent;
		bucketCurrent = bucketCurrent->hnext;
	}
	if (bucketCurrent != NULL) {
		if (bucketPrevious == NULL)
			hashTable[index] = current->hnext;
		else
			bucketPrevious->hnext = current->hnext;
	}

	free(current);
	printf("\nProfile deleted successfully.\n");
}

/* Searches only one bucket of the hash table instead of the whole list. */
void searchBloodGroup(void)
{
	char blood[5];
	struct Profile *current;
	int found = 0;

	if (profileHead == NULL) {
		printf("\nNo profiles registered yet.\n");
		printf("Register donors or receivers before searching.\n");
		return;
	}

	readBloodGroup(blood);
	printf("\n========== BLOOD GROUP SEARCH ==========\n");
	printf("Searching for blood group: %s\n\n", blood);

	current = hashTable[hashFunction(blood)];
	while (current != NULL) {
		/* bucket may also hold other blood groups (collision) */
		if (strcmp(current->blood, blood) == 0) {
			showProfile(current, "Profile Type", "----------------------------------------");
			found = 1;
		}
		current = current->hnext;
	}

	if (!found)
		printf("No profiles found for blood group %s.\n", blood);
}

/* ---------- PRIORITY QUEUE: urgent requests (sorted array) ----------
   The array is kept sorted from highest priority (3) to lowest (1).
   Index 0 is always the request to process next. */

void addUrgentRequest(void)
{
	struct Request request;
	int i;

	if (urgentCount == MAX) {
		printf("\nUrgent request queue is full.\n");
		return;
	}

	if (!readRequest(&request, &nextUrgentId))
		return;

	request.priority = readInt("Urgency (1-Low, 2-Medium, 3-High): ");
	while (request.priority < 1 || request.priority > 3) {
		printf("Enter 1, 2, or 3.\n");
		request.priority = readInt("Urgency (1-Low, 2-Medium, 3-High): ");
	}

	/* Start at the end and shift lower-priority requests one place right.
	   We stop at an EQUAL priority, so equal requests keep arrival order. */
	i = urgentCount;
	while (i > 0 && urgentRequests[i - 1].priority < request.priority) {
		urgentRequests[i] = urgentRequests[i - 1];
		i--;
	}
	urgentRequests[i] = request;
	urgentCount++;

	printf("\nUrgent blood request registered successfully!\n");
	printf("Request ID: %d | Patient: %s\n", request.id, request.name);
	printf("Blood Group: %s | Units: %d | Priority: %d\n",
	       request.blood, request.units, request.priority);
}

void processUrgentRequest(void)
{
	struct Request request;

	if (urgentCount == 0) {
		printf("\nNo urgent blood requests are pending.\n");
		return;
	}

	request = popFront(urgentRequests, &urgentCount);

	printf("\n========== PROCESSING URGENT REQUEST ==========\n");
	printf("Request ID: %d\n", request.id);
	printf("Patient: %s\n", request.name);
	printf("Blood Group: %s | Units: %d\n", request.blood, request.units);
	printf("Priority: %d\n", request.priority);
	printf("Urgent request processed from the queue.\n");
}

void displayUrgentRequests(void)
{
	int i;

	if (urgentCount == 0) {
		printf("\nNo urgent blood requests are pending.\n");
		return;
	}

	printf("\n========== URGENT BLOOD REQUESTS ==========\n");
	for (i = 0; i < urgentCount; i++) {
		printf("Request ID: %d | Patient: %s\n", urgentRequests[i].id, urgentRequests[i].name);
		printf("Blood Group: %s | Units: %d | Priority: %d\n",
		       urgentRequests[i].blood, urgentRequests[i].units, urgentRequests[i].priority);
		printf("--------------------------------------------\n");
	}
}

/* ---------- CIRCULAR QUEUE: donation history ----------
   historyFront = index of the oldest entry
   historyCount = number of entries
   Position of the i-th entry = (historyFront + i) % MAX, which wraps
   around to 0 after MAX - 1, so freed slots are reused. */

void displayHistory(void)
{
	int i, position;

	if (historyCount == 0) {
		printf("\nNo donation history available.\n");
		return;
	}

	printf("\n========== DONOR HISTORY LOG ==========\n");
	for (i = 0; i < historyCount; i++) {
		position = (historyFront + i) % MAX;
		showDonation(&history[position], "---------------------------------------");
	}
}

/* DEQUEUE the oldest entry */
void removeOldestHistory(void)
{
	if (historyCount == 0) {
		printf("\nNo donation history to remove.\n");
		return;
	}

	printf("\nRemoving oldest history entry:\n");
	printf("Donation ID: %d | Donor: %s | Blood: %s\n",
	       history[historyFront].id, history[historyFront].name, history[historyFront].blood);

	historyFront = (historyFront + 1) % MAX;
	historyCount--;
	printf("Oldest history entry removed successfully.\n");
}

/* ---------- BINARY SEARCH TREE: blood bank branches ----------
   Smaller IDs go to the left, larger IDs go to the right. */

struct Branch *findBranch(int id)
{
	struct Branch *current = branchRoot;

	while (current != NULL) {
		if (id == current->id)
			return current;
		if (id < current->id)
			current = current->left;
		else
			current = current->right;
	}
	return NULL;
}

void addBranch(void)
{
	char name[50];
	struct Branch *newBranch;
	struct Branch *current;
	struct Branch *parent = NULL;
	int id = readInt("\nBranch ID: ");

	if (findBranch(id) != NULL) {
		printf("\nA branch with this ID already exists.\n");
		return;
	}

	if (!readName("Branch Name: ", name, sizeof(name), "Branch name cannot be empty."))
		return;

	newBranch = malloc(sizeof(struct Branch));
	if (newBranch == NULL) {
		printf("\nMemory allocation failed.\n");
		return;
	}
	newBranch->id = id;
	strcpy(newBranch->name, name);
	newBranch->left = NULL;
	newBranch->right = NULL;

	if (branchRoot == NULL) {
		branchRoot = newBranch;
	} else {
		/* Walk down to the empty spot, remembering the parent. */
		current = branchRoot;
		while (current != NULL) {
			parent = current;
			if (id < current->id)
				current = current->left;
			else
				current = current->right;
		}
		if (id < parent->id)
			parent->left = newBranch;
		else
			parent->right = newBranch;
	}

	printf("\nBlood bank branch added successfully.\n");
}

/* Inorder traversal (left, node, right) prints IDs in ascending order.
   Recursion has no fixed-size array, so it works for any number of branches. */
void printBranchesInOrder(struct Branch *node)
{
	if (node == NULL)
		return;

	printBranchesInOrder(node->left);
	printf("Branch ID: %d | Branch Name: %s\n", node->id, node->name);
	printBranchesInOrder(node->right);
}

void displayBranches(void)
{
	if (branchRoot == NULL) {
		printf("\nNo blood bank branches registered.\n");
		return;
	}

	printf("\n========== BLOOD BANK BRANCHES ==========\n");
	printBranchesInOrder(branchRoot);
}

void searchBranch(void)
{
	int id;
	struct Branch *branch;

	if (branchRoot == NULL) {
		printf("\nNo branches registered yet.\n");
		return;
	}

	id = readInt("\nEnter branch ID to search: ");
	branch = findBranch(id);
	if (branch == NULL) {
		printf("\nNo branch found with ID %d.\n", id);
		return;
	}

	printf("\n========== BRANCH FOUND ==========\n");
	printf("Branch ID: %d\n", branch->id);
	printf("Branch Name: %s\n", branch->name);
}

/* ---------- cleanup ---------- */

/* Postorder: free both children before the node itself. */
void freeBranches(struct Branch *node)
{
	if (node == NULL)
		return;

	freeBranches(node->left);
	freeBranches(node->right);
	free(node);
}

void cleanup(void)
{
	struct Profile *current = profileHead;
	struct Profile *nextProfile;
	int i;

	/* Free each profile once through the main list.
	   The hash table points to the same nodes, so it is only cleared. */
	while (current != NULL) {
		nextProfile = current->next;
		free(current);
		current = nextProfile;
	}
	profileHead = NULL;
	for (i = 0; i < HASH_SIZE; i++)
		hashTable[i] = NULL;

	freeBranches(branchRoot);
	branchRoot = NULL;
}

/* ---------- menus ---------- */

void donationMenu(void)
{
	int choice;

	do {
		printf("\n========== DONATION MANAGEMENT ==========\n");
		printf("1. Record Donation\n");
		printf("2. Remove Latest Donation from Stack\n");
		printf("3. View Donation Records\n");
		printf("4. Return to Main Menu\n");

		choice = readInt("\nEnter choice: ");
		switch (choice) {
		case 1: addDonation(); break;
		case 2: removeDonation(); break;
		case 3: displayDonations(); break;
		case 4: break;
		default: printf("\nInvalid choice. Try again.\n");
		}

		if (choice != 4)
			pauseScreen();
	} while (choice != 4);
}

void requestMenu(void)
{
	int choice;

	do {
		printf("\n========== BLOOD REQUEST MANAGEMENT ==========\n");
		printf("1. Register Regular Blood Request\n");
		printf("2. Process Oldest Regular Request\n");
		printf("3. View Pending Regular Requests\n");
		printf("4. Return to Main Menu\n");

		choice = readInt("\nEnter choice: ");
		switch (choice) {
		case 1: addRequest(); break;
		case 2: processRequest(); break;
		case 3: displayRequests(); break;
		case 4: break;
		default: printf("\nInvalid choice. Try again.\n");
		}

		if (choice != 4)
			pauseScreen();
	} while (choice != 4);
}

void profileMenu(void)
{
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
		case 1: addProfile(); break;
		case 2: displayProfiles(); break;
		case 3: searchProfileById(); break;
		case 4: deleteProfile(); break;
		case 5: break;
		default: printf("\nInvalid choice. Try again.\n");
		}

		if (choice != 5)
			pauseScreen();
	} while (choice != 5);
}

void urgentMenu(void)
{
	int choice;

	do {
		printf("\n========== URGENT BLOOD REQUESTS ==========\n");
		printf("1. Register Urgent Request\n");
		printf("2. Process Highest-Priority Request\n");
		printf("3. View Urgent Requests\n");
		printf("4. Return to Main Menu\n");

		choice = readInt("\nEnter choice: ");
		switch (choice) {
		case 1: addUrgentRequest(); break;
		case 2: processUrgentRequest(); break;
		case 3: displayUrgentRequests(); break;
		case 4: break;
		default: printf("\nInvalid choice. Try again.\n");
		}

		if (choice != 4)
			pauseScreen();
	} while (choice != 4);
}

void historyMenu(void)
{
	int choice;

	do {
		printf("\n========== DONOR HISTORY LOG ==========\n");
		printf("1. View Donation History\n");
		printf("2. Remove Oldest History Entry\n");
		printf("3. Return to Main Menu\n");

		choice = readInt("\nEnter choice: ");
		switch (choice) {
		case 1: displayHistory(); break;
		case 2: removeOldestHistory(); break;
		case 3: break;
		default: printf("\nInvalid choice. Try again.\n");
		}

		if (choice != 3)
			pauseScreen();
	} while (choice != 3);
}

void branchMenu(void)
{
	int choice;

	do {
		printf("\n========== BLOOD BANK BRANCHES ==========\n");
		printf("1. Register Branch\n");
		printf("2. View All Branches\n");
		printf("3. Search Branch by ID\n");
		printf("4. Return to Main Menu\n");

		choice = readInt("\nEnter choice: ");
		switch (choice) {
		case 1: addBranch(); break;
		case 2: displayBranches(); break;
		case 3: searchBranch(); break;
		case 4: break;
		default: printf("\nInvalid choice. Try again.\n");
		}

		if (choice != 4)
			pauseScreen();
	} while (choice != 4);
}

int main(void)
{
	int choice;

	do {
		printf("\n============================================\n");
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
		case 1: donationMenu(); break;
		case 2: requestMenu(); break;
		case 3: profileMenu(); break;
		case 4: urgentMenu(); break;
		case 5: historyMenu(); break;
		case 6: branchMenu(); break;
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
