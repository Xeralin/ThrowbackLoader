#include "Uplay.h"

#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>

#include <chrono>
#include <list>

#define EXTRA_SAVE_PADDING (32 + 8)

struct FileData
{
    char* file_name;
    uint32_t* unknown;
    uint32_t file_size;
    uint32_t padding;
    uint64_t last_modified;
};

struct FileList
{
    uint32_t number;
    uint32_t padding = 0;
    struct FileData** files;
};

DLLEXPORT int UPC_StorageFileClose(ContextData* context, unsigned in_handle)
{
    LOGGER_INFO(__FUNCTION__);
    return _close(in_handle);
}

DLLEXPORT int UPC_StorageFileDelete(ContextData* context, char* in_file_name_utf8)
{
    LOGGER_INFO(__FUNCTION__);

    std::error_code error_code;
    if (std::filesystem::remove(get_saves_path(context->config->GetAppId()) / in_file_name_utf8, error_code))
    {
        return 0;
    }
    return -1;
}

DLLEXPORT int UPC_StorageFileListFree(ContextData* context, FileList* in_storage_file_list)
{
    LOGGER_INFO(__FUNCTION__);
    if (!in_storage_file_list)
    {
        return -0xd;
    }

    if (in_storage_file_list->number)
    {
        for (unsigned i = 0; i < in_storage_file_list->number; ++i)
        {
            delete[] in_storage_file_list->files[i]->file_name;
            delete in_storage_file_list->files[i]->unknown;
            delete in_storage_file_list->files[i];
        }
    }

    delete[] in_storage_file_list->files;
    delete in_storage_file_list;
    return 0;
}

DLLEXPORT int UPC_StorageFileListGet(ContextData* context, FileList** out_storage_file_list)
{
    LOGGER_INFO(__FUNCTION__);
    if (!context || !out_storage_file_list)
    {
        return -0xd;
    }

    std::error_code error_code;
    const auto directory = std::filesystem::directory_iterator(get_saves_path(context->config->GetAppId()), error_code);
    if (error_code)
    {
        return -0xd;
    }

    std::list<std::filesystem::directory_entry> list;
    for (const auto& file : directory)
    {
        if (file.is_regular_file(error_code))
        {
            list.push_back(file);
        }
    }

    struct FileList* fl = new FileList();
    fl->number = static_cast<uint32_t>(list.size());
    fl->files = new FileData*[list.size()];
    unsigned index = 0;
    for (const auto& file : list)
    {
        const uintmax_t file_size = std::filesystem::file_size(file, error_code);
        if (error_code || file_size < EXTRA_SAVE_PADDING)
        {
            --fl->number;
            continue;
        }

        const auto last_write_time = file.last_write_time(error_code);
        if (error_code)
        {
            --fl->number;
            continue;
        }

        const std::string file_name = file.path().filename().string();

        FileData* f_data = new FileData();
        f_data->file_name = new char[file_name.size() + 1];
        file_name.copy(f_data->file_name, file_name.size());
        f_data->file_name[file_name.size()] = 0;
        f_data->unknown = new uint32_t;
        *f_data->unknown = 0;
        f_data->padding = 0;
        f_data->file_size = static_cast<uint32_t>(file_size - EXTRA_SAVE_PADDING);
        const auto unix_time = std::chrono::clock_cast<std::chrono::system_clock>(last_write_time);
        f_data->last_modified =
            std::chrono::duration_cast<std::chrono::milliseconds>(unix_time.time_since_epoch()).count();
        fl->files[index] = f_data;
        ++index;
    }

    *out_storage_file_list = fl;
    return 0;
}

DLLEXPORT int UPC_StorageFileOpen(ContextData* context, char* in_file_name_utf8, unsigned in_flags, int* out_handle)
{
    LOGGER_INFO(__FUNCTION__);
    int oflag = _O_BINARY | _O_CREAT | _O_RDWR;
    if (in_flags == 0x2)
    {
        oflag |= _O_TRUNC;
    }

    const auto file_path = get_saves_path(context->config->GetAppId()) / in_file_name_utf8;

    int file_handle = -1;
    const errno_t open_error =
        _sopen_s(&file_handle, file_path.string().c_str(), oflag, _SH_DENYNO, _S_IREAD | _S_IWRITE);
    if (open_error != 0)
    {
        return -1;
    }

    *out_handle = file_handle;
    return 0;
}

DLLEXPORT int UPC_StorageFileRead(ContextData* context, int in_handle, int in_bytes_to_read,
                                  unsigned in_bytes_read_offset, void* out_data, int* out_bytes_read, void* in_callback,
                                  void* in_callback_data)
{
    LOGGER_INFO(__FUNCTION__);

    int out = 0;
    if (_lseek(in_handle, in_bytes_read_offset + EXTRA_SAVE_PADDING, SEEK_SET) < 0)
    {
        *out_bytes_read = 0;
        out = -1;
    }
    else
    {
        const int ret = _read(in_handle, out_data, in_bytes_to_read);
        if (ret >= 0)
        {
            *out_bytes_read = ret;
        }
        else
        {
            *out_bytes_read = 0;
            out = -1;
        }
    }

    context->callbacks.push(CallbackData(in_callback, in_callback_data, out));
    return 0x10000;
}

DLLEXPORT int UPC_StorageFileWrite(ContextData* context, int in_handle, void* in_data, int in_size, void* in_callback,
                                   void* in_callback_data)
{
    LOGGER_INFO(__FUNCTION__);

    char padding_data[EXTRA_SAVE_PADDING] = {};

    int out = 0;
    if (_write(in_handle, padding_data, sizeof(padding_data)) != static_cast<int>(sizeof(padding_data)))
    {
        out = -1;
    }
    else if (_write(in_handle, in_data, in_size) != in_size)
    {
        out = -1;
    }

    context->callbacks.push(CallbackData(in_callback, in_callback_data, out));
    return 0x10000;
}