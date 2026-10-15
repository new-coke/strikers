#ifndef RE4_GX_COMMAND_BUFFER_H
#define RE4_GX_COMMAND_BUFFER_H
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

// One producer owns a buffer until publication; one consumer owns it until completion.
class Re4GXCommandBuffer {
    struct alignas(16) Header {
        void (*execute)(void *);
        uint32_t bytes;
    };
    uint8_t *data_;
    size_t capacity_, used_ = 0;
    static size_t aligned(size_t n) { return (n + 15) & ~size_t(15); }
public:
    Re4GXCommandBuffer(void *data = nullptr, size_t capacity = 0) : data_(static_cast<uint8_t *>(data)), capacity_(capacity) {}
    void reset_storage(void *data, size_t capacity) {
        data_ = static_cast<uint8_t *>(data);
        capacity_ = capacity;
        used_ = 0;
    }
    size_t size() const { return used_; }
    size_t remaining() const { return capacity_ - used_; }
    template<class T> static constexpr size_t command_bytes(size_t extra = 0) {
        return (sizeof(Header) + sizeof(T) + extra + 15) & ~size_t(15);
    }
    template<class T, class... Args> T *append(size_t extra, Args&&... args) {
        static_assert(alignof(T) <= 16, "Command alignment");
        if (extra > capacity_ || sizeof(Header) + sizeof(T) > capacity_ - extra) return nullptr;
        const size_t bytes = aligned(sizeof(Header) + sizeof(T) + extra);
        if (bytes > capacity_ - used_) return nullptr;
        auto *h = new(data_ + used_) Header{[](void *p) { auto *command = static_cast<T *>(p); (*command)(); command->~T(); }, static_cast<uint32_t>(bytes)};
        auto *result = new(h + 1) T(std::forward<Args>(args)...);
        used_ += bytes;
        return result;
    }
    void execute() {
        for (size_t offset = 0; offset < used_;) {
            auto *h = reinterpret_cast<Header *>(data_ + offset);
            const size_t bytes = h->bytes;
            h->execute(h + 1);
            offset += bytes;
        }
        used_ = 0;
    }
};
#endif
