#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tinyxml2.h"

#define SAFE_FREE(p) { if (p) {free((void *)p); p = NULL;} }
#define SAFE_STR(s) ((s) == NULL ? "" : s)
#define SAFE_DUP(s) ((s) == NULL ? NULL : strdup(s))
#define INIT_STR(s) (memset(s, 0, sizeof(s)))
#define UUID_MAX (36)

extern "C" {
    #include "macserial.h"
    int serial_main(int argc, char *argv[], char *, char *);
    uint32_t arc4random(void);
#ifdef __APPLE__
    #include "modelinfo.h"
    int32_t get_current_model(void);
#endif

#ifdef _WIN32
    #include <objbase.h>
#elif defined(__APPLE__)
    #include <CoreFoundation/CFUUID.h>
#endif
}

typedef struct {
    char *boardver;
    char *sn;
    char *boardsn;
    char *smuuid;
} info_nodes;

static void free_info_nodes(info_nodes *nodes) {
    SAFE_FREE(nodes->boardver);
    SAFE_FREE(nodes->sn);
    SAFE_FREE(nodes->boardsn);
    SAFE_FREE(nodes->smuuid);
}

static void usage(char *app) {
    fprintf(stderr, "Usage: %s [options]\n", app);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -t <template>   config.plist template\n");
    fprintf(stderr, "  -i <source>     use information from existing config.plist\n");
    fprintf(stderr, "  -o <output>     write to output file\n");
}

static info_nodes find_nodes(char *source_path) {
    info_nodes ret = {0};
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(source_path) != tinyxml2::XML_SUCCESS) {
        fprintf(stderr, "Error: cannot parse file: %s\n", source_path);
        return ret;
    }
    tinyxml2::XMLElement *root = doc.FirstChildElement("plist");
    if (!root) {
        return ret;
    }
    tinyxml2::XMLElement *dict = root->FirstChildElement("dict");
    if (!dict) {
        return ret;
    }
    tinyxml2::XMLElement *smbios = NULL;
    for (tinyxml2::XMLElement* curr = dict->FirstChildElement();
         curr != NULL;
         curr = curr->NextSiblingElement()) {
        if (strcmp(curr->Name(), "key") == 0) {
            if (strcmp(curr->GetText(), "SMBIOS") == 0) {
                smbios = curr->NextSiblingElement();
                break;
            }
        }
    }
    if (smbios == NULL) {
        return ret;
    }
    for (tinyxml2::XMLElement* curr = smbios->FirstChildElement();
         curr != NULL;
         curr = curr->NextSiblingElement()) {
        if (strcmp(curr->Name(), "key") == 0) {
            tinyxml2::XMLElement *value_node = curr->NextSiblingElement();
            const char *value = value_node->GetText();
            if (strcmp(curr->GetText(), "BoardVersion") == 0) {
                ret.boardver = SAFE_DUP(value);
            } else if (strcmp(curr->GetText(), "BoardSerialNumber") == 0) {
                ret.boardsn = SAFE_DUP(value);
            } else if (strcmp(curr->GetText(), "SerialNumber") == 0) {
                ret.sn = SAFE_DUP(value);
            } else if (strcmp(curr->GetText(), "SmUUID") == 0) {
                ret.smuuid = SAFE_DUP(value);
            }
        }
    }
    return ret;
}

static int write_nodes(char *template_path, FILE *output_file, info_nodes *info) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(template_path) != tinyxml2::XML_SUCCESS) {
        fprintf(stderr, "Error: cannot parse file: %s\n", template_path);
        return -1;
    }
    tinyxml2::XMLElement *root = doc.FirstChildElement("plist");
    if (!root) {
        return -1;
    }
    tinyxml2::XMLElement *dict = root->FirstChildElement("dict");
    if (!dict) {
        return -1;
    }
    tinyxml2::XMLElement *smbios = NULL;
    for (tinyxml2::XMLElement* curr = dict->FirstChildElement();
         curr != NULL;
         curr = curr->NextSiblingElement()) {
        if (strcmp(curr->Name(), "key") == 0) {
            if (strcmp(curr->GetText(), "SMBIOS") == 0) {
                smbios = curr->NextSiblingElement();
                break;
            }
        }
    }
    if (smbios == NULL) {
        return -1;
    }
    for (tinyxml2::XMLElement* curr = smbios->FirstChildElement();
         curr != NULL;
         curr = curr->NextSiblingElement()) {
        if (strcmp(curr->Name(), "key") == 0) {
            tinyxml2::XMLElement *value_node = curr->NextSiblingElement();
            if (strcmp(curr->GetText(), "BoardSerialNumber") == 0) {
                value_node->SetText(SAFE_STR(info->boardsn));
            } else if (strcmp(curr->GetText(), "SerialNumber") == 0) {
                value_node->SetText(SAFE_STR(info->sn));
            } else if (strcmp(curr->GetText(), "SmUUID") == 0) {
                value_node->SetText(SAFE_STR(info->smuuid));
            }
        }
    }
    tinyxml2::XMLPrinter printer(output_file);
    doc.Print(&printer);
    return 0;
}

void uuid_gen_random(char *uuid) {
    if (uuid == NULL) {
        return;
    }
    sprintf(uuid, "%04X%04X-%04X-%04X-%04X-%04X%04X%04X",
                  (unsigned) (arc4random() & 0xffff),
                  (unsigned) (arc4random() & 0xffff),
                  (unsigned) (arc4random() & 0xffff),
                  (unsigned) ((arc4random() & 0x0fff) | 0x4000),
                  (unsigned) ((arc4random() & 0x3fff) | 0x8000),
                  (unsigned) (arc4random() & 0xffff),
                  (unsigned) (arc4random() & 0xffff),
                  (unsigned) (arc4random() & 0xffff));
}

void uuid_gen(char *uuid) {
    if (uuid == NULL) {
        return;
    }
#if defined(__APPLE__) || defined(_WIN32)
#ifdef _WIN32
    GUID newId;
    CoCreateGuid(&newId);
    uint8_t bytes[16] = {
        (unsigned char)((newId.Data1 >> 24) & 0xFF),
        (unsigned char)((newId.Data1 >> 16) & 0xFF),
        (unsigned char)((newId.Data1 >> 8) & 0xFF),
        (unsigned char)((newId.Data1) & 0xff),
        (unsigned char)((newId.Data2 >> 8) & 0xFF),
        (unsigned char)((newId.Data2) & 0xff),
        (unsigned char)((newId.Data3 >> 8) & 0xFF),
        (unsigned char)((newId.Data3) & 0xFF),
        (unsigned char)newId.Data4[0],
        (unsigned char)newId.Data4[1],
        (unsigned char)newId.Data4[2],
        (unsigned char)newId.Data4[3],
        (unsigned char)newId.Data4[4],
        (unsigned char)newId.Data4[5],
        (unsigned char)newId.Data4[6],
        (unsigned char)newId.Data4[7]
    };
#endif
#ifdef __APPLE__
    CFUUIDRef newId = CFUUIDCreate(kCFAllocatorDefault);
    CFUUIDBytes uuid_bytes = CFUUIDGetUUIDBytes(newId);
    CFRelease(newId);
    uint8_t bytes[16] = {
        uuid_bytes.byte0,
        uuid_bytes.byte1,
        uuid_bytes.byte2,
        uuid_bytes.byte3,
        uuid_bytes.byte4,
        uuid_bytes.byte5,
        uuid_bytes.byte6,
        uuid_bytes.byte7,
        uuid_bytes.byte8,
        uuid_bytes.byte9,
        uuid_bytes.byte10,
        uuid_bytes.byte11,
        uuid_bytes.byte12,
        uuid_bytes.byte13,
        uuid_bytes.byte14,
        uuid_bytes.byte15
    };
#endif
    sprintf(uuid, "%02X%02X%02X%02X" "-" 
                  "%02X%02X" "-"
                  "%02X%02X" "-"
                  "%02X%02X" "-"
                  "%02X%02X%02X%02X%02X%02X",
            bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5],
            bytes[6], bytes[7],
            bytes[8], bytes[9],
            bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
#else
    return uuid_gen_random(uuid);
#endif
}

int main(int argc, char *argv[]) {
    int i;
    char *source_path = NULL;
    char *output_path = NULL;
    char *template_path = NULL;
    char **path = NULL;
    FILE *output_file = NULL;
    int next_arg = 0;
    info_nodes info = {0};

    for (i = 0; i < argc; i++) {
        if (argv[i][0] == '-') {
            next_arg = 1;
            switch (argv[i][1]) {
                case 't':
                    path = &template_path;
                    break;
                case 'i':
                    path = &source_path;
                    break;
                case 'o':
                    path = &output_path;
                    break;
            }
        } else {
            if (next_arg) {
                *path = strdup(argv[i]);
                next_arg = 0;
            }
        }
    }
    if (template_path == NULL) {
        fprintf(stderr, "Error: no template config.plist specified.\n");
        usage(argv[0]);
        goto end;
    }
    if (output_path == NULL) {
        fprintf(stderr, "Error: need to specify output file.\n");
        usage(argv[0]);
        goto end;
    }
    if (source_path) {
        fprintf(stdout, "Using information from existing config.plist\n");
        info = find_nodes(source_path);
        if (info.sn == NULL) {
            fprintf(stdout, "Error: failed to fetch information: %s\n", source_path);
            goto end;
        }
    }
    if (info.boardver == NULL) {
        info = find_nodes(template_path);
    }
    if (info.boardver == NULL) {
#ifdef __APPLE__
        i = get_current_model();
        if (i < APPLE_MODEL_MAX && i >= 0) {
            info.boardver = strdup(ApplePlatformData[i].productName);
            fprintf(stdout, "Detecting current model: %s\n", info.boardver);
        } else
#endif
        {
            fprintf(stderr, "Error: BoardVersion not specified.\n");
            goto end;
        }
    }
    if (!source_path) {
        char serial_result_sn[MLB_MAX_SIZE + 1];
        char serial_result_mlb[MLB_MAX_SIZE + 1];
        INIT_STR(serial_result_mlb);
        INIT_STR(serial_result_sn);
        char const *serial_argv[] = {
            "macserial", "-n", "1", "-m", info.boardver, NULL
        };
        int serial_argc = sizeof(serial_argv) / sizeof(char *) - 1;
        int result = serial_main(serial_argc, (char **)serial_argv,
                                 serial_result_sn, serial_result_mlb);
        if (result != EXIT_SUCCESS) {
            fprintf(stderr, "Error: failed to generate serial numbers.\n");
            goto end;
        }
        info_nodes gen_info = {0};
        gen_info.boardsn = strdup(serial_result_mlb);
        gen_info.sn = strdup(serial_result_sn);
        gen_info.smuuid = (char *)malloc(UUID_MAX + 1);
        uuid_gen(gen_info.smuuid);
        free_info_nodes(&info);
        info = gen_info;
    }
    fprintf(stdout, "Generated Information:\n  SN: %s\n  Board SN: %s\n  UUID: %s\n",
            SAFE_STR(info.sn),
            SAFE_STR(info.boardsn),
            SAFE_STR(info.smuuid));
    output_file = fopen(output_path, "w");
    if (output_file == NULL) {
        fprintf(stderr, "Error: cannot write to output: %s\n", output_path);
    } else {
        if (write_nodes(template_path, output_file, &info) == 0) {
            fprintf(stdout, "Generated: %s\n", output_path);
        } else {
            fprintf(stderr, "Error: template file is invalid.");
        }
    }
end:
    SAFE_FREE(source_path);
    SAFE_FREE(output_path);
    SAFE_FREE(template_path);
    free_info_nodes(&info);
    if (output_file) {
        fclose(output_file);
    }
    return 0;
}
