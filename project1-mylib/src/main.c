#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bitmap.h"
#include "hash.h"
#include "list.h"
#include "debug.h"

#define MAX_STRUCTURES 10
#define MAX_LINE 100

// Global variables
struct list *lists[MAX_STRUCTURES];
struct hash *hashes[MAX_STRUCTURES];
struct bitmap *bitmaps[MAX_STRUCTURES];

/**
 * Extracts the numerical index from a named data structure identifier.
 * Supports "list#", "hash#", and "bm#" name formats.
 *
 * 자료구조 이름에서 인덱스를 추출하는 함수.
 * "list#", "hash#", "bm#" 형식의 이름을 지원함.
 * 이름 뒤의 숫자를 정수로 변환하여 반환함.
 */
int get_index(const char *name) {
    if (!strncmp(name, "list", 4)) return atoi(name + 4);
    if (!strncmp(name, "hash", 4)) return atoi(name + 4);
    if (!strncmp(name, "bm", 2)) return atoi(name + 2);
    return -1;
}

int main() {
    char line[MAX_LINE];
    char cmd[MAX_LINE];

    // 랜덤 시드 main 함수 시작할 때 설정
    srand((unsigned int)time(NULL));
    
    while (fgets(line, MAX_LINE, stdin)) {
        // 줄바꿈 문자 제거
        line[strcspn(line, "\n")] = 0;
        
        // 명령어 파싱
        if (sscanf(line, "%s", cmd) != 1) continue;
        
        if (!strcmp(cmd, "quit")) {
            break;
        }
        // List, hash, bitmap 모두 해당되는 명렁어들
        else if (!strcmp(cmd, "create")) {
            char type[20];
            char name[20];
            sscanf(line, "%*s %s %s", type, name);
            
            int idx = get_index(name);
            if (idx < 0 || idx >= MAX_STRUCTURES) continue;
            
            if (!strcmp(type, "list")) {
                lists[idx] = malloc(sizeof(struct list));
                list_init(lists[idx]);
            }
            else if (!strcmp(type, "hashtable")) {
                hashes[idx] = malloc(sizeof(struct hash));
                hash_init(hashes[idx], hash_func, less_hash, NULL);
            }
            else if (!strcmp(type, "bitmap")) {
                int size;
                sscanf(line, "%*s %*s %*s %d", &size);
                bitmaps[idx] = bitmap_create(size);
            }
        }
        else if (!strcmp(cmd, "dumpdata")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            if (!strncmp(name, "list", 4)) {
                print_list(lists[idx]);
            }
            else if (!strncmp(name, "hash", 4)) {
                print_hash(hashes[idx]);
            }
            else if (!strncmp(name, "bm", 2)) {
                print_bitmap(bitmaps[idx]);
            }
        }
        else if (!strcmp(cmd, "delete")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            if (!strncmp(name, "list", 4)) {
                struct list_elem *e;
                while (!list_empty(lists[idx])) {
                    e = list_pop_front(lists[idx]);
                    free(list_entry(e, struct list_item, elem));
                }
                free(lists[idx]);
            }
            else if (!strncmp(name, "hash", 4)) {
                hash_destroy(hashes[idx], NULL);
            }
            else if (!strncmp(name, "bm", 2)) {
                bitmap_destroy(bitmaps[idx]);
            }
        }
        
        // 리스트 관련 명령어
        else if (!strcmp(cmd, "list_push_back")) {
            char name[20];
            int value;
            sscanf(line, "%*s %s %d", name, &value);
            int idx = get_index(name);
            
            struct list_item *new_elem = malloc(sizeof(struct list_item));
            new_elem->value = value;
            list_push_back(lists[idx], &new_elem->elem);
        }
        else if (!strcmp(cmd, "list_push_front")) {
            char name[20];
            int value;
            sscanf(line, "%*s %s %d", name, &value);
            int idx = get_index(name);
            
            struct list_item *new_elem = malloc(sizeof(struct list_item));
            new_elem->value = value;
            list_push_front(lists[idx], &new_elem->elem);
        }
        else if (!strcmp(cmd, "list_pop_back")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            struct list_elem *e = list_pop_back(lists[idx]);
            free(list_entry(e, struct list_item, elem));
        }
        else if (!strcmp(cmd, "list_pop_front")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            struct list_elem *e = list_pop_front(lists[idx]);
            free(list_entry(e, struct list_item, elem));
        }
        else if (!strcmp(cmd, "list_insert")) {
            char name[20];
            int pos, value;
            sscanf(line, "%*s %s %d %d", name, &pos, &value);
            int idx = get_index(name);
            
            struct list_elem *e = list_begin(lists[idx]);
            for (int i = 0; i < pos && e != list_end(lists[idx]); i++) {
                e = list_next(e);
            }
            
            struct list_item *new_elem = malloc(sizeof(struct list_item));
            new_elem->value = value;
            list_insert(e, &new_elem->elem);
        }
        else if (!strcmp(cmd, "list_insert_ordered")) {
            char name[20];
            int value;
            sscanf(line, "%*s %s %d", name, &value);
            int idx = get_index(name);
            
            struct list_item *new_elem = malloc(sizeof(struct list_item));
            new_elem->value = value;
            list_insert_ordered(lists[idx], &new_elem->elem, less_list, NULL);
        }
        else if (!strcmp(cmd, "list_empty")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            printf("%s\n", list_empty(lists[idx]) ? "true" : "false");
        }
        else if (!strcmp(cmd, "list_size")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            printf("%zu\n", list_size(lists[idx]));
        }
        else if (!strcmp(cmd, "list_front")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            struct list_elem *e = list_front(lists[idx]);
            printf("%d\n", list_entry(e, struct list_item, elem)->value);
        }
        else if (!strcmp(cmd, "list_back")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            struct list_elem *e = list_back(lists[idx]);
            printf("%d\n", list_entry(e, struct list_item, elem)->value);
        }
        else if (!strcmp(cmd, "list_remove")) {
            char name[20];
            int pos;
            sscanf(line, "%*s %s %d", name, &pos);
            int idx = get_index(name);
            
            struct list_elem *e = list_begin(lists[idx]);
            for (int i = 0; i < pos && e != list_end(lists[idx]); i++) {
                e = list_next(e);
            }
            if (e != list_end(lists[idx])) {
                list_remove(e);
                free(list_entry(e, struct list_item, elem));
            }
        }
        else if (!strcmp(cmd, "list_sort")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            list_sort(lists[idx], less_list, NULL);
        }
        else if (!strcmp(cmd, "list_reverse")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            list_reverse(lists[idx]);
        }
        else if (!strcmp(cmd, "list_swap")) {
            char name[20];
            int pos1, pos2;
            sscanf(line, "%*s %s %d %d", name, &pos1, &pos2);
            int idx = get_index(name);
            
            struct list_elem *e1 = list_begin(lists[idx]);
            struct list_elem *e2 = list_begin(lists[idx]);
            
            for (int i = 0; i < pos1 && e1 != list_end(lists[idx]); i++) {
                e1 = list_next(e1);
            }
            for (int i = 0; i < pos2 && e2 != list_end(lists[idx]); i++) {
                e2 = list_next(e2);
            }
            
            if (e1 != list_end(lists[idx]) && e2 != list_end(lists[idx])) {
                list_swap(e1, e2);
            }
        }
        else if (!strcmp(cmd, "list_splice")) {
            char name1[20], name2[20];
            int before_pos, first_pos, last_pos;
            sscanf(line, "%*s %s %d %s %d %d", name1, &before_pos, name2, &first_pos, &last_pos);
            int idx1 = name1[strlen(name1) - 1] - '0';
            int idx2 = name2[strlen(name2) - 1] - '0';
            
            struct list_elem *before = list_begin(lists[idx1]);
            struct list_elem *first = list_begin(lists[idx2]);
            struct list_elem *last = list_begin(lists[idx2]);
            
            for (int i = 0; i < before_pos && before != list_end(lists[idx1]); i++) {
                before = list_next(before);
            }
            for (int i = 0; i < first_pos && first != list_end(lists[idx2]); i++) {
                first = list_next(first);
            }
            for (int i = 0; i < last_pos && last != list_end(lists[idx2]); i++) {
                last = list_next(last);
            }
            
            if (before != list_end(lists[idx1]) && first != list_end(lists[idx2]) && last != list_end(lists[idx2])) {
                list_splice(before, first, last);
            }
        }
        else if (!strcmp(cmd, "list_unique")) {
            char name1[20], name2[20];
            int args = sscanf(line, "%*s %s %s", name1, name2);
            int idx1 = name1[strlen(name1) - 1] - '0';
            
            if (args == 1 || !strcmp(name2, "null")) {
                // 인자가 하나만 있거나 두 번째 인자가 null인 경우
                list_unique(lists[idx1], NULL, less_list, NULL);
            } else {
                // 두 번째 인자가 존재하고 null이 아닌 경우
                int idx2 = name2[strlen(name2) - 1] - '0';
                list_unique(lists[idx1], lists[idx2], less_list, NULL);
            }
        }
        else if (!strcmp(cmd, "list_max")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            if (!list_empty(lists[idx])) {
                struct list_elem *max = list_max(lists[idx], less_list, NULL);
                printf("%d\n", list_entry(max, struct list_item, elem)->value);
            }
        }
        else if (!strcmp(cmd, "list_min")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            if (!list_empty(lists[idx])) {
                struct list_elem *min = list_min(lists[idx], less_list, NULL);
                printf("%d\n", list_entry(min, struct list_item, elem)->value);
            }
        }
        else if (!strcmp(cmd, "list_maxminsizeempty")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            if (!list_empty(lists[idx])) {
                struct list_elem *max = list_max(lists[idx], less_list, NULL);
                struct list_elem *min = list_min(lists[idx], less_list, NULL);
                printf("max = %d\n", list_entry(max, struct list_item, elem)->value);
                printf("min = %d\n", list_entry(min, struct list_item, elem)->value);
            }
            printf("size = %zu\n", list_size(lists[idx]));
            printf("empty = %s\n", list_empty(lists[idx]) ? "true" : "false");
        }
        else if (!strcmp(cmd, "list_shuffle")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);

            list_shuffle(lists[idx]);
        }
        // 해시테이블 관련 명령어
        else if (!strcmp(cmd, "hash_insert")) {
            char name[20];
            int value;
            sscanf(line, "%*s %s %d", name, &value);
            int idx = get_index(name);
            
            struct hash_elem_item *new_item = malloc(sizeof(struct hash_elem_item));
            new_item->value = value;
            hash_insert(hashes[idx], &new_item->elem);
        }
        else if (!strcmp(cmd, "hash_replace")) {
            char name[20];
            int value;
            sscanf(line, "%*s %s %d", name, &value);
            int idx = get_index(name);
            
            struct hash_elem_item *new_item = malloc(sizeof(struct hash_elem_item));
            new_item->value = value;
            struct hash_elem *old = hash_replace(hashes[idx], &new_item->elem);
            if (old != NULL) {
                free(hash_entry(old, struct hash_elem_item, elem));
            }
        }
        else if (!strcmp(cmd, "hash_find")) {
            char name[20];
            int value;
            sscanf(line, "%*s %s %d", name, &value);
            int idx = get_index(name);
            
            struct hash_elem_item temp;
            temp.value = value;
            struct hash_elem *found = hash_find(hashes[idx], &temp.elem);
            if (found != NULL) {
                printf("%d\n", hash_entry(found, struct hash_elem_item, elem)->value);
            }
        }
        else if (!strcmp(cmd, "hash_delete")) {
            char name[20];
            int value;
            sscanf(line, "%*s %s %d", name, &value);
            int idx = get_index(name);
            
            struct hash_elem_item temp;
            temp.value = value;
            struct hash_elem *deleted = hash_delete(hashes[idx], &temp.elem);
            if (deleted != NULL) {
                free(hash_entry(deleted, struct hash_elem_item, elem));
            }
        }
        else if (!strcmp(cmd, "hash_clear")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            hash_clear(hashes[idx], NULL);
        }
        else if (!strcmp(cmd, "hash_size")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            printf("%zu\n", hash_size(hashes[idx]));
        }
        else if (!strcmp(cmd, "hash_empty")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            printf("%s\n", hash_empty(hashes[idx]) ? "true" : "false");
        }
        else if (!strcmp(cmd, "hash_apply")) {
            char name[20], action[20];
            sscanf(line, "%*s %s %s", name, action);
            int idx = get_index(name);
            
            if (!strcmp(action, "square")) {
                hash_apply(hashes[idx], square);
            }
            else if (!strcmp(action, "triple")) {
                hash_apply(hashes[idx], triple);
            }
        }
        // 비트맵 관련 명령어
        else if (!strcmp(cmd, "bitmap_mark")) {
            char name[20];
            size_t bit_idx;
            sscanf(line, "%*s %s %zu", name, &bit_idx);
            int idx = get_index(name);
            
            bitmap_mark(bitmaps[idx], bit_idx);
        }
        else if (!strcmp(cmd, "bitmap_reset")) {
            char name[20];
            size_t bit_idx;
            sscanf(line, "%*s %s %zu", name, &bit_idx);
            int idx = get_index(name);
            
            bitmap_reset(bitmaps[idx], bit_idx);
        }
        else if (!strcmp(cmd, "bitmap_flip")) {
            char name[20];
            size_t bit_idx;
            sscanf(line, "%*s %s %zu", name, &bit_idx);
            int idx = get_index(name);
            
            bitmap_flip(bitmaps[idx], bit_idx);
        }
        else if (!strcmp(cmd, "bitmap_test")) {
            char name[20];
            size_t bit_idx;
            sscanf(line, "%*s %s %zu", name, &bit_idx);
            int idx = get_index(name);
            
            printf("%s\n", bitmap_test(bitmaps[idx], bit_idx) ? "true" : "false");
        }
        else if (!strcmp(cmd, "bitmap_set")) {
            char name[20];
            size_t idx;
            char value_str[10];
            bool value;
            
            sscanf(line, "%*s %s %zu %s", name, &idx, value_str);
            int name_idx = name[strlen(name) - 1] - '0';
            
            if (!strcmp(value_str, "true")) {
                value = true;
            } else {
                value = false;
            }
            
            bitmap_set(bitmaps[name_idx], idx, value);
        }
        else if (!strcmp(cmd, "bitmap_set_all")) {
            char name[20];
            char value_str[10];
            bool value;
            
            sscanf(line, "%*s %s %s", name, value_str);
            int idx = get_index(name);
            
            if (!strcmp(value_str, "true")) {
                value = true;
            } else {
                value = false;
            }
            
            bitmap_set_all(bitmaps[idx], value);
        }
        else if (!strcmp(cmd, "bitmap_set_multiple")) {
            char name[20];
            size_t start, cnt;
            char value_str[10];
            bool value;
            
            sscanf(line, "%*s %s %zu %zu %s", name, &start, &cnt, value_str);
            int idx = get_index(name);
            
            if (!strcmp(value_str, "true")) {
                value = true;
            } else {
                value = false;
            }
            
            bitmap_set_multiple(bitmaps[idx], start, cnt, value);
        }
        else if (!strcmp(cmd, "bitmap_contains")) {
            char name[20];
            size_t start, cnt;
            bool value;
            char value_str[10];
            sscanf(line, "%*s %s %zu %zu %s", name, &start, &cnt, value_str);
            if (!strcmp(value_str, "true")) {
                value = true;
            } else if (!strcmp(value_str, "false")) {
                value = false;
            } else {
                printf("Invalid value: must be true or false\n");
                return 1;
            }

            int idx = get_index(name);

            if (cnt == 0) {
                printf("true\n");
            } else if (start >= bitmap_size(bitmaps[idx]) || start + cnt > bitmap_size(bitmaps[idx])) {
                // 범위 초과 검사
                printf("false\n");
            } else if (bitmap_contains(bitmaps[idx], start, cnt, value)) {
                printf("true\n");
            } else {
                printf("false\n");
            }
        }
        else if (!strcmp(cmd, "bitmap_count")) {
            char name[20];
            size_t start, cnt;
            bool value;
            char value_str[10];
            sscanf(line, "%*s %s %zu %zu %s", name, &start, &cnt, value_str);
            if (!strcmp(value_str, "true")) {
                value = true;
            } else if (!strcmp(value_str, "false")) {
                value = false;
            } else {
                printf("Invalid value: must be true or false\n");
                return 1;
            }
            int idx = get_index(name);
            
            printf("%zu\n", bitmap_count(bitmaps[idx], start, cnt, value));
        }
        else if (!strcmp(cmd, "bitmap_any")) {
            char name[20];
            size_t start, cnt;
            sscanf(line, "%*s %s %zu %zu", name, &start, &cnt);
            int idx = get_index(name);
            
            printf("%s\n", bitmap_any(bitmaps[idx], start, cnt) ? "true" : "false");
        }
        else if (!strcmp(cmd, "bitmap_none")) {
            char name[20];
            size_t start, cnt;
            sscanf(line, "%*s %s %zu %zu", name, &start, &cnt);
            int idx = get_index(name);
            
            printf("%s\n", bitmap_none(bitmaps[idx], start, cnt) ? "true" : "false");
        }
        else if (!strcmp(cmd, "bitmap_all")) {
            char name[20];
            size_t start, cnt;
            sscanf(line, "%*s %s %zu %zu", name, &start, &cnt);
            int idx = get_index(name);
            
            printf("%s\n", bitmap_all(bitmaps[idx], start, cnt) ? "true" : "false");
        }
        else if (!strcmp(cmd, "bitmap_scan")) {
            char name[20];
            size_t start, cnt;
            bool value;
            char value_str[10];
            sscanf(line, "%*s %s %zu %zu %s", name, &start, &cnt, value_str);
            if (!strcmp(value_str, "true")) {
                value = true;
            } else if (!strcmp(value_str, "false")) {
                value = false;
            } else {
                printf("Invalid value: must be true or false\n");
                return 1;
            }
            int idx = get_index(name);
            
            if (cnt == 0) {
                printf("%zu\n", start);
            } else if (start >= bitmap_size(bitmaps[idx])) {
                printf("BITMAP_ERROR\n");
            } else if (start + cnt > bitmap_size(bitmaps[idx])) {
                printf("BITMAP_ERROR\n");
            } else {
                size_t result = bitmap_scan(bitmaps[idx], start, cnt, value);
                printf("%zu\n", result);
            }
        }
        else if (!strcmp(cmd, "bitmap_scan_and_flip")) {
            char name[20];
            size_t start, cnt;
            bool value;
            char value_str[10];
            sscanf(line, "%*s %s %zu %zu %s", name, &start, &cnt, value_str);
            if (!strcmp(value_str, "true")) {
                value = true;
            } else if (!strcmp(value_str, "false")) {
                value = false;
            } else {
                printf("Invalid value: must be true or false\n");
                return 1;
            }
            int idx = get_index(name);
            
            if (cnt == 0) {
                printf("%zu\n", start);
            } else if (start >= bitmap_size(bitmaps[idx])) {
                printf("BITMAP_ERROR\n");
            } else if (start + cnt > bitmap_size(bitmaps[idx])) {
                printf("BITMAP_ERROR\n");
            } else {
                size_t result = bitmap_scan_and_flip(bitmaps[idx], start, cnt, value);
                printf("%zu\n", result);
            }
        }
        else if (!strcmp(cmd, "bitmap_dump")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            bitmap_dump(bitmaps[idx]);
        }
        else if (!strcmp(cmd, "bitmap_expand")) {
            char name[20];
            size_t size;
            sscanf(line, "%*s %s %zu", name, &size);
            int idx = get_index(name);

            bitmaps[idx] = bitmap_expand(bitmaps[idx], size);
        }
        else if (!strcmp(cmd, "bitmap_size")) {
            char name[20];
            sscanf(line, "%*s %s", name);
            int idx = get_index(name);
            
            printf("%zu\n", bitmap_size(bitmaps[idx]));
        }
    }
    return 0;
} 