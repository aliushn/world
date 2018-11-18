#ifndef SHADOW_CORE_WORKSPACE_HPP
#define SHADOW_CORE_WORKSPACE_HPP

#include "blob.hpp"
#include "context.hpp"

#include <map>
#include <memory>
#include <string>
#include <typeinfo>

namespace Shadow {

static const std::string int_id(typeid(int).name());
static const std::string float_id(typeid(float).name());
static const std::string uchar_id(typeid(unsigned char).name());

class Workspace {
 public:
  Workspace() = default;
  ~Workspace() {
    for (auto blob_it : blob_map_) {
      const auto &blob_type = blob_it.second.first;
      auto *blob = blob_it.second.second;
      ClearBlob(blob_type, blob);
    }
    blob_map_.clear();
  }

  bool HasBlob(const std::string &name) const;

  const std::string GetBlobType(const std::string &name) const;

  template <typename T>
  const Blob<T> *GetBlob(const std::string &name) const {
    if (blob_map_.count(name)) {
      const auto &blob_type = blob_map_.at(name).first;
      const auto ask_type = typeid(T).name();
      CHECK(blob_type == ask_type) << "Blob " << name << " has type "
                                   << blob_type << ", but ask for " << ask_type;
      return static_cast<const Blob<T> *>(blob_map_.at(name).second);
    }
    DLOG(WARNING) << "Blob " << name << " not in the workspace.";
    return nullptr;
  }
  template <typename T>
  Blob<T> *GetBlob(const std::string &name) {
    return const_cast<Blob<T> *>(
        static_cast<const Workspace *>(this)->GetBlob<T>(name));
  }

  template <typename T>
  Blob<T> *CreateBlob(const std::string &name) {
    return CreateBlob<T>({}, name);
  }
  template <typename T>
  Blob<T> *CreateBlob(const VecInt &shape, const std::string &name,
                      bool shared = false) {
    if (HasBlob(name)) {
      if (!shape.empty()) {
        CHECK(shape == GetBlob<T>(name)->shape())
            << "Blob " << name << " shape mismatch";
      }
    } else {
      blob_map_[name].first = typeid(T).name();
      if (!shape.empty()) {
        blob_map_[name].second = new Blob<T>(shape, name, shared);
      } else {
        blob_map_[name].second = new Blob<T>(name);
      }
    }
    return GetBlob<T>(name);
  }

  template <typename Dtype>
  Blob<Dtype> *CreateTempBlob(const VecInt &shape, const std::string &name) {
    Blob<Dtype> *blob = nullptr;
    if (!HasBlob(name)) {
      blob_map_[name].first = typeid(Dtype).name();
      blob_map_[name].second = new Blob<Dtype>(name);
    }
    blob = GetBlob<Dtype>(name);
    CHECK_NOTNULL(blob);
    blob->clear();
    blob->set_shape(shape);
    size_t cou = 1;
    for (const auto dim : shape) cou *= dim;
    CHECK_GT(cou, 0);
    int size = sizeof(Dtype) / sizeof(unsigned char);
    blob->share_data(reinterpret_cast<Dtype *>(GetTempPtr(cou, size)), shape);
    return blob;
  }

  void GrowTempBuffer(int count, int elem_size);

  size_t GetWorkspaceSize() const;
  size_t GetWorkspaceTempSize() const;

  void CreateCtx(int device_id);
  Context *Ctx();

 private:
  void ClearBlob(const std::string &blob_type, void *blob);

  void *GetTempPtr(size_t count, int elem_size);

  std::map<std::string, std::pair<std::string, void *>> blob_map_;
  std::shared_ptr<BlobUC> blob_temp_ = nullptr;
  size_t temp_offset_ = 0;

  std::shared_ptr<Context> context_ = nullptr;

  DISABLE_COPY_AND_ASSIGN(Workspace);
};

}  // namespace Shadow

#endif  // SHADOW_CORE_WORKSPACE_HPP
