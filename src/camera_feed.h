#ifndef CAMERA_FEED_H
#define CAMERA_FEED_H

#include <memory>

#include "godot_cpp/classes/camera_feed.hpp"

using namespace godot;

class CameraFeedExtension;

namespace extension {
class CameraFeed {
protected:
	CameraFeedExtension *this_;
	int selected_format = -1;

	virtual void set_this(CameraFeedExtension *feed);

public:
	CameraFeed();
	CameraFeed(CameraFeedExtension *feed);
	virtual ~CameraFeed();

	virtual bool set_format(int p_index, const Dictionary &p_parameters);
	virtual TypedArray<Dictionary> get_formats() const;

	virtual bool activate_feed();
	virtual void deactivate_feed();

	friend class ::CameraFeedExtension;
};
} // namespace extension

class CameraFeedExtension : public CameraFeed {
	GDCLASS(CameraFeedExtension, CameraFeed)

private:
	std::unique_ptr<extension::CameraFeed> impl;

protected:
	static void _bind_methods();

public:
	CameraFeedExtension();
	CameraFeedExtension(std::unique_ptr<extension::CameraFeed> impl);
	~CameraFeedExtension();

	bool set_format(int p_index, const Dictionary &p_parameters);
	TypedArray<Dictionary> get_formats() const;

	bool _activate_feed() override;
	void _deactivate_feed() override;

	extension::CameraFeed *get_impl();
};

#endif
