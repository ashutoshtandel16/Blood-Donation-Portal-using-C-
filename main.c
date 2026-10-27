#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define MAX 50
#define HASH_SIZE 10

struct Profile {
	int id;
	char name[50], blood[5], type[10];
	struct Profile *next;   /* linked list */
	struct Profile *hnext;  /* hash-bucket chain */
};

struct Donation {           /* used for both the stack and the history log */
	int id, donorProfileId;
	char name[50], blood[5];
	int units;
};

struct Request {            /* priority is unused (0) for regular requests */
	int id;
	char name[50], blood[5];
	int units, priority;
};

struct Branch {
	int id;
	char name[50];
	struct Branch *left, *right;
};

struct Menu {
	const char *title;
	int n;
	const char *items[4];
	void (*fn[4])(void);
};

struct Donation donations[MAX];          /* stack */
int donationTop = -1;
struct Request requests[MAX];            /* queue */
int requestCount = 0;
struct Profile *profileHead = NULL;      /* linked list */
struct Request urgentRequests[MAX];      /* priority queue */
int urgentCount = 0;
struct Donation history[MAX];            /* circular queue */
int historyFront = 0, historyCount = 0;
struct Branch *branchRoot = NULL;        /* BST */
struct Profile *hashTable[HASH_SIZE];    /* hash table, chaining */
int nextDonationId = 1, nextRequestId = 1, nextUrgentId = 1;

/* ---------- input utilities ---------- */

void readLine(const char *prompt, char buffer[], int size) {
	int ch;
	size_t len;

	printf("%s", prompt);
	if (fgets(buffer, size, stdin) == NULL) 
	{
		printf("\nInput ended. Exiting safely.\n");
		exit(EXIT_FAILURE);
	}
	len = strlen(buffer);
	if (len > 0 && buffer[len - 1] == '\n')
		buffer[len - 1] = '\0';
	else
		while ((ch = getchar()) != '\n' && ch != EOF) { }
}

int readInt(const char *prompt) {
	char buffer[100], *end;
	long value;

	while (1) {
		readLine(prompt, buffer, sizeof(buffer));
		errno = 0;
		value = strtol(buffer, &end, 10);
		while (*end == ' ' || *end == '\t') end++;
		if (end == buffer || *end != '\0' || errno == ERANGE ||
		        value < INT_MIN || value > INT_MAX) {
			printf("Invalid input. Enter a whole number.\n");
			continue;
		}
		return (int)value;
	}
}

int readPositiveInt(const char *prompt) {
	int v;
	while ((v = readInt(prompt)) <= 0)
		printf("Please enter a value greater than zero.\n");
	return v;
}

void readBloodGroup(char blood[]) {
	const char *const groups[] = {"A+", "A-", "B+", "B-", "AB+", "AB-", "O+", "O-"};
	char input[30];
	int i;

	while (1) {
		readLine("Blood Group (A+, A-, B+, B-, AB+, AB-, O+, O-): ", input, sizeof(input));
		for (i = 0; input[i]; i++)
			input[i] = (char)toupper((unsigned char)input[i]);
		for (i = 0; i < 8; i++)
			if (strcmp(input, groups[i]) == 0) {
				strcpy(blood, input);
				return;
			}
		printf("Invalid blood group. Please try again.\n");
	}
}

/* reads a name; prints err and returns 0 if it is empty */
int readName(const char *prompt, char buf[], int size, const char *err) {
	readLine(prompt, buf, size);
	if (buf[0]) return 1;
	printf("%s\n", err);
	return 0;
}

void pauseScreen(void) {
	char buffer[10];
	readLine("\nPress Enter to continue...", buffer, sizeof(buffer));
}

/* ---------- stack: donations (LIFO) ---------- */

struct Profile *findProfileById(int id) {
	struct Profile *p = profileHead;
	while (p && p->id != id) p = p->next;
	return p;
}

void showDonation(const struct Donation *d, const char *sep) {
	printf("Donation ID: %d | Donor ID: %d\n"
	       "Donor: %s | Blood Group: %s | Units: %d\n%s\n",
	       d->id, d->donorProfileId, d->name, d->blood, d->units, sep);
}

void addDonation(void) {
	struct Donation entry;
	struct Profile *donor;

	if (donationTop == MAX - 1) {
		printf("\nDonation stack is full.\n");
		return;
	}
	donor = findProfileById(readInt("\nRegistered donor profile ID: "));
	if (donor == NULL || strcmp(donor->type, "Donor") != 0) {
		printf("\n No registered donor found with that ID.\n");
		printf("Register the donor before recording a donation.\n");
		return;
	}
	entry.id = nextDonationId++;
	entry.donorProfileId = donor->id;
	strcpy(entry.name, donor->name);
	strcpy(entry.blood, donor->blood);
	entry.units = readPositiveInt("Units donated: ");
	donations[++donationTop] = entry;

	printf("\nDonation recorded successfully.\n");
	printf("Donation ID: %d\n", entry.id);
	printf("Donor: %s | Blood Group: %s | Units: %d\n", entry.name, entry.blood, entry.units);
}

void removeDonation(void) {
	struct Donation entry;

	if (donationTop == -1) {
		printf("\nNo donations are available.\n");
		return;
	}
	entry = donations[donationTop--];
	printf("\nLatest donation removed from the stack:\n");
	printf("Donation ID: %d | Donor: %s | Blood: %s | Units: %d\n",
	       entry.id, entry.name, entry.blood, entry.units);

	if (historyCount == MAX) {
		printf("History log is full. The oldest history entry "
		       "must be removed before a new one can be recorded.\n");
		return;
	}
	history[(historyFront + historyCount++) % MAX] = entry;
	printf("Donation added to the donor history log.\n");
}

void displayDonations(void) {
	int i;

	if (donationTop == -1) {
		printf("\nNo donations have been recorded.\n");
		return;
	}
	printf("\n========== DONATION RECORDS ==========\n");
	for (i = donationTop; i >= 0; i--)
		showDonation(&donations[i],
		             "--------------------------------------");
}

/* ---------- queue: regular requests (FIFO) ---------- */

/* shared by regular and urgent requests; returns 0 if cancelled */
int readRequest(struct Request *e, int *nextId) {
	e->id = (*nextId)++;
	if (!readName("\nPatient Name: ", e->name, sizeof(e->name),
	              "Patient name cannot be empty."))

	{
		(*nextId)--;
		return 0;
	}
	readBloodGroup(e->blood);
	e->units = readPositiveInt("Units required: ");
	return 1;
}

struct Request popFront(struct Request q[], int *count)

{
	struct Request e = q[0];
	memmove(q, q + 1, (--*count) * sizeof(*q));
	return e;
}

void addRequest(void)
{
	struct Request e;

	if (requestCount == MAX)
	{
		printf("\nRequest queue is full.\n");
		return;
	}

	if (!readRequest(&e, &nextRequestId)) return;
	requests[requestCount++] = e;
	printf("\nBlood request registered successfully.\n");
	printf("Request ID: %d | Patient: %s\n", e.id, e.name);
}

void processRequest(void)

{
	struct Request e;

	if (requestCount == 0)

	{
		printf("\nThere are no pending regular requests.\n");
		return;
	}
	e = popFront(requests, &requestCount);
	printf("\nProcessing oldest blood request:\n");
	printf("Request ID: %d | Patient: %s\n", e.id, e.name);
	printf("Blood Group: %s | Units: %d\n", e.blood, e.units);
	printf("Request processed from the queue.\n");
}

void displayRequests(void)
{
	int i;

	if (requestCount == 0)
	{
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

/* ---------- linked list + hash table: profiles ---------- */

int hashFunction(const char blood[])
{
	unsigned int sum = 0;
	while (*blood) sum += (unsigned char)*blood++;
	return (int)(sum % HASH_SIZE);
}

void showProfile(const struct Profile *p,
                 const char *typeLabel, const char *sep)
{
	printf("Profile ID: %d\nName: %s\nBlood Group: %s\n%s: %s\n",
	       p->id, p->name, p->blood, typeLabel, p->type);
	if (sep) printf("%s\n", sep);
}

void addProfile(void)
{
	struct Profile *p, **slot;
	int id = readInt("\nEnter unique profile ID: ");

	if (findProfileById(id) != NULL) {
		printf("\nA profile with this ID already exists.\n");
		return;
	}
	if ((p = calloc(1, sizeof(*p))) == NULL) {
		printf("\nUnable to allocate memory for profile.\n");
		return;
	}

	p->id = id;
	if (!readName("Full Name: ", p->name, sizeof(p->name), "Name cannot be empty.")) {
		free(p);
		return;
	}

	readBloodGroup(p->blood);

	while (1)
	{
		readLine("Profile Type (Donor/Receiver): ", p->type, sizeof(p->type));
		if (strcmp(p->type, "donor") == 0) strcpy(p->type, "Donor");
		else if (strcmp(p->type, "receiver") == 0) strcpy(p->type, "Receiver");
		if (strcmp(p->type, "Donor") == 0 || strcmp(p->type, "Receiver") == 0) break;
		printf("Please enter Donor or Receiver.\n");
	}

	for (slot = &profileHead; *slot; slot = &(*slot)->next)
	{ }
	*slot = p;
	for (slot = &hashTable[hashFunction(p->blood)];
	        *slot; slot = &(*slot)->hnext)
	{ }
	*slot = p;

	printf("\nProfile registered successfully!\n");
	printf("Profile ID: %d\n", p->id);
	printf("Name: %s | Blood Group: %s | Type: %s\n", p->name, p->blood, p->type);
}

void displayProfiles(void)
{
	struct Profile *p = profileHead;

	if (p == NULL)
	{
		printf("\nNo donor or receiver profiles registered yet.\n");
		return;
	}
	printf("\n========== REGISTERED PROFILES ==========\n");
	for (; p; p = p->next)
		showProfile(p, "Profile Type",
		            "-----------------------------------------");
}

void searchProfileById(void)
{
	int id = readInt("\nEnter profile ID: ");
	struct Profile *p = findProfileById(id);

	if (p == NULL)
	{
		printf("\nNo profile found with ID %d.\n", id);
		return;
	}

	printf("\n========== PROFILE DETAILS ==========\n");
	showProfile(p, "Type", NULL);
}

void deleteProfile(void)
{
	struct Profile *p, **pp = &profileHead, **h;
	int i, id = readInt("\nEnter profile ID to delete: ");

	while (*pp && (*pp)->id != id) pp = &(*pp)->next;
	if ((p = *pp) == NULL)
	{
		printf("\nProfile not found.\n");
		return;
	}
	/* do not delete a profile that donation records still refer to */

	for (i = 0; i <= donationTop; i++)
		if (donations[i].donorProfileId == id)
		{
			printf("\nCannot delete this profile while its donation is in the active stack.\n");
			return;
		}

	for (i = 0; i < historyCount; i++)
		if (history[(historyFront + i) % MAX].donorProfileId == id)

		{
			printf("\nCannot delete this profile because its donation history is retained.\n");
			return;
		}

	*pp = p->next;
	for (h = &hashTable[hashFunction(p->blood)]; *h != p; h = &(*h)->hnext)
	{ }

	*h = p->hnext;
	free(p);
	printf("\nProfile deleted successfully.\n");
}

void searchBloodGroup(void)

{
	char blood[5];
	struct Profile *p;
	int found = 0;

	if (profileHead == NULL)
	{
		printf("\nNo profiles registered yet.\n");
		printf("Register donors or receivers before searching.\n");
		return;
	}

	readBloodGroup(blood);
	printf("\n========== BLOOD GROUP SEARCH ==========\n");
	printf("Searching for blood group: %s\n\n", blood);
	for (p = hashTable[hashFunction(blood)];
	        p;
	        p = p->hnext)
		if (strcmp(p->blood, blood) == 0)
		{
			showProfile(p, "Profile Type",
			            "----------------------------------------");
			found = 1;
		}
	if (!found)
		printf("No profiles found for blood group %s.\n", blood);
}

/* ---------- priority queue: urgent requests ---------- */

void addUrgentRequest(void)
{
	struct Request e;
	int i;

	if (urgentCount == MAX)
	{
		printf("\nUrgent request queue is full.\n");
		return;
	}
	if (!readRequest(&e, &nextUrgentId))

		return;
	while ((e.priority = readInt("Urgency (1-Low, 2-Medium, 3-High): ")) < 1 || e.priority > 3)
		printf("Enter 1, 2, or 3.\n");

	/* descending priority; equal priorities keep arrival order */
	for (i = urgentCount; i > 0 && urgentRequests[i - 1].priority < e.priority; i--)
		urgentRequests[i] = urgentRequests[i - 1];
	urgentRequests[i] = e;
	urgentCount++;

	printf("\nUrgent blood request registered successfully!\n");
	printf("Request ID: %d | Patient: %s\n", e.id, e.name);
	printf("Blood Group: %s | Units: %d | Priority: %d\n", e.blood, e.units, e.priority);
}

void processUrgentRequest(void)
{
	struct Request e;

	if (urgentCount == 0)
	{
		printf("\nNo urgent blood requests are pending.\n");
		return;
	}

	e = popFront(urgentRequests, &urgentCount);
	printf("\n========== PROCESSING URGENT REQUEST ==========\n");
	printf("Request ID: %d\n", e.id);
	printf("Patient: %s\n", e.name);
	printf("Blood Group: %s | Units: %d\n", e.blood, e.units);
	printf("Priority: %d\n", e.priority);
	printf("Urgent request processed from the queue.\n");
}

void displayUrgentRequests(void)
{
	int i;

	if (urgentCount == 0)
	{
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

/* ---------- circular queue: donor history ---------- */

void displayHistory(void)
{
	int i;

	if (historyCount == 0)
	{
		printf("\nNo donation history available.\n");
		return;
	}
	printf("\n========== DONOR HISTORY LOG ==========\n");
	for (i = 0; i < historyCount; i++)
		showDonation(&history[(historyFront + i) % MAX],
		             "---------------------------------------");
}

void removeOldestHistory(void)
{
	if (historyCount == 0)
	{
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

/* ---------- BST: blood bank branches ---------- */

/* returns the link where branch `id` is, or where it would be inserted */
struct Branch **branchSlot(int id) {
	struct Branch **pp = &branchRoot;
	while (*pp && (*pp)->id != id)
		pp = id < (*pp)->id ? &(*pp)->left : &(*pp)->right;
	return pp;
}

void addBranch(void)
{
	char name[50];
	struct Branch *b, **slot;
	int id = readInt("\nBranch ID: ");

	slot = branchSlot(id);
	if (*slot)
	{
		printf("\nA branch with this ID already exists.\n");
		return;
	}

	if (!readName("Branch Name: ", name, sizeof(name), "Branch name cannot be empty."))
		return;
	if ((b = calloc(1, sizeof(*b))) == NULL) {
		printf("\nMemory allocation failed.\n");
		return;
	}

	b->id = id;
	strcpy(b->name, name);
	*slot = b;
	printf("\nBlood bank branch added successfully.\n");
}

void displayBranches(void)
{
	struct Branch *stack[MAX], *cur = branchRoot;
	int top = -1;

	if (branchRoot == NULL)
	{
		printf("\nNo blood bank branches registered.\n");
		return;
	}
	printf("\n========== BLOOD BANK BRANCHES ==========\n");
	while (cur != NULL || top >= 0)
	{	/* iterative inorder: ascending ID */
		while (cur != NULL)
		{
			if (top == MAX - 1)
			{
				printf("Tree traversal stack capacity exceeded.\n");
				return;
			}

			stack[++top] = cur;
			cur = cur->left;
		}
		cur = stack[top--];
		printf("Branch ID: %d | Branch Name: %s\n", cur->id, cur->name);
		cur = cur->right;
	}
}

void searchBranch(void)
{
	int id;
	struct Branch *b;

	if (branchRoot == NULL)
	{
		printf("\nNo branches registered yet.\n");
		return;
	}

	id = readInt("\nEnter branch ID to search: ");
	if ((b = *branchSlot(id)) == NULL)
	{
		printf("\nNo branch found with ID %d.\n", id);
		return;
	}

	printf("\n========== BRANCH FOUND ==========\n");
	printf("Branch ID: %d\n", b->id);
	printf("Branch Name: %s\n", b->name);
}

/* ---------- cleanup ---------- */

void freeBranches(struct Branch *root)
{
	if (root == NULL) return;
	freeBranches(root->left);
	freeBranches(root->right);
	free(root);
}

void cleanup(void)
{
	struct Profile *next;

	for (; profileHead; profileHead = next)
	{	/* hash nodes are the same objects */
		next = profileHead->next;
		free(profileHead);
	}

	freeBranches(branchRoot);
	branchRoot = NULL;
}

/* ---------- menus ---------- */

const struct Menu menus[] =
{
	{	"DONATION MANAGEMENT", 3,
		{"Record Donation", "Remove Latest Donation from Stack", "View Donation Records"},
		{addDonation, removeDonation, displayDonations}
	},

	{	"BLOOD REQUEST MANAGEMENT", 3,
		{	"Register Regular Blood Request", "Process Oldest Regular Request",
			"View Pending Regular Requests"
		},
		{addRequest, processRequest, displayRequests}
	},

	{	"DONOR & RECEIVER PROFILES", 4,
		{"Register New Profile", "View All Profiles", "Search Profile by ID", "Delete Profile"},
		{addProfile, displayProfiles, searchProfileById, deleteProfile}
	},

	{	"URGENT BLOOD REQUESTS", 3,
		{"Register Urgent Request", "Process Highest-Priority Request", "View Urgent Requests"},
		{addUrgentRequest, processUrgentRequest, displayUrgentRequests}
	},

	{	"DONOR HISTORY LOG", 2,
		{"View Donation History", "Remove Oldest History Entry"},
		{displayHistory, removeOldestHistory}
	},

	{	"BLOOD BANK BRANCHES", 3,
		{"Register Branch", "View All Branches", "Search Branch by ID"},
		{addBranch, displayBranches, searchBranch}
	}
};

void runMenu(const struct Menu *m)
{
	int i, choice;

	do {
		printf("\n========== %s ==========\n", m->title);
		for (i = 0; i < m->n; i++)
			printf("%d. %s\n", i + 1, m->items[i]);
		printf("%d. Return to Main Menu\n", m->n + 1);

		choice = readInt("\nEnter choice: ");
		if (choice >= 1 && choice <= m->n)
			m->fn[choice - 1]();
		else if (choice != m->n + 1)
			printf("\nInvalid choice. Try again.\n");

		if (choice != m->n + 1)
			pauseScreen();
	} while (choice != m->n + 1);
}

int main(void)
{
	const char *const items[] =
	{
		"Donation Management", "Blood Request Management", "Donor & Receiver Profiles",
		"Urgent Blood Requests", "Donor History Log", "Blood Bank Branches",
		"Search Profiles by Blood Group", "Exit Portal"
	};

	int i, choice;

	do {
		printf("\n============================================\n"
		       "       BLOOD DONATION & REQUEST PORTAL      \n"
		       "============================================\n"
		       " Supporting SDG 3: Good Health and Well-being\n"
		       "--------------------------------------------\n");
		for (i = 0; i < 8; i++)
			printf(" %d. %s\n", i + 1, items[i]);

		printf("--------------------------------------------\n");

		choice = readInt("Enter your choice: ");
		if (choice >= 1 && choice <= 6)
		{
			runMenu(&menus[choice - 1]);
		}
		else if (choice == 7) {
			searchBloodGroup();
			pauseScreen();
		}
		else if (choice == 8) {
			printf("\nThank you for using the portal.\n");
			printf("Donate blood. Support life.\n");
			cleanup();

		}
		else
		{
			printf("\nInvalid choice. Please select 1-8.\n");
			pauseScreen();
		}

	}
	while (choice != 8);

	return 0;
}
