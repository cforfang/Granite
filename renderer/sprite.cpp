#include "sprite.hpp"
#include "device.hpp"
#include "render_context.hpp"
#include <string.h>

using namespace Util;

namespace Granite
{
namespace RenderFunctions
{
void sprite_render(Vulkan::CommandBuffer &cmd, const RenderInfo **infos, unsigned num_instances)
{
	auto &info = *static_cast<const SpriteRenderInfo *>(infos[0]);
	cmd.set_program(*info.program);

	if (info.texture)
	{
		float inv_res[2] = {
			1.0f / info.texture->get_image().get_create_info().width,
			1.0f / info.texture->get_image().get_create_info().height,
		};
		cmd.push_constants(inv_res, 0, sizeof(inv_res));
		cmd.set_texture(2, 0, *info.texture, Vulkan::StockSampler::NearestWrap);
	}

	cmd.set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
	auto *quad = static_cast<int8_t *>(cmd.allocate_vertex_data(0, 8, 2));
	quad[0] = 0;
	quad[1] = 1;
	quad[2] = 1;
	quad[3] = 1;
	quad[4] = 0;
	quad[5] = 0;
	quad[6] = 1;
	quad[7] = 0;

	unsigned quads = 0;
	for (unsigned i = 0; i < num_instances; i++)
		quads += static_cast<const SpriteRenderInfo *>(infos[i])->quad_count;

	auto *data = static_cast<SpriteRenderInfo::QuadData *>(
		cmd.allocate_vertex_data(1, quads * sizeof(SpriteRenderInfo::QuadData),
		                         sizeof(SpriteRenderInfo::QuadData), VK_VERTEX_INPUT_RATE_INSTANCE));

	quads = 0;
	for (unsigned i = 0; i < num_instances; i++)
	{
		auto &info = *static_cast<const SpriteRenderInfo *>(infos[i]);
		memcpy(data + quads, info.quads, info.quad_count * sizeof(*data));
		quads += info.quad_count;
	}
	cmd.set_vertex_attrib(0, 0, VK_FORMAT_R8G8_SINT, 0);
	cmd.set_vertex_attrib(1, 1, VK_FORMAT_R16G16B16A16_SINT, offsetof(SpriteRenderInfo::QuadData, pos_off_x));
	cmd.set_vertex_attrib(2, 1, VK_FORMAT_R16G16B16A16_SINT, offsetof(SpriteRenderInfo::QuadData, tex_off_x));
	cmd.set_vertex_attrib(3, 1, VK_FORMAT_R8G8B8A8_UNORM, offsetof(SpriteRenderInfo::QuadData, color));
	cmd.set_vertex_attrib(4, 1, VK_FORMAT_R32_SFLOAT, offsetof(SpriteRenderInfo::QuadData, layer));
	cmd.draw(4, quads);
}
}
void Sprite::get_quad_render_info(const vec3 &position, RenderQueue &queue) const
{
	auto queue_type = pipeline == MeshDrawPipeline::AlphaBlend ? Queue::Transparent : Queue::Opaque;
	auto &sprite = queue.emplace<SpriteRenderInfo>(queue_type);

	static const uint32_t uv_mask = 1u << ecast(MeshAttribute::UV);
	static const uint32_t pos_mask = 1u << ecast(MeshAttribute::Position);
	static const uint32_t base_color_mask = 1u << ecast(Material::Textures::BaseColor);

	sprite.program = queue.get_shader_suites()[ecast(RenderableType::Quad)].get_program(pipeline,
	                                                                                    texture ? (uv_mask | pos_mask) : pos_mask,
	                                                                                    texture ? base_color_mask : 0).get();
	if (texture)
		sprite.texture = &texture->get_image()->get_view();

	sprite.quads = static_cast<SpriteRenderInfo::QuadData *>(queue.allocate(sizeof(SpriteRenderInfo::QuadData)));
	sprite.quad_count = 1;

	for (unsigned i = 0; i < 4; i++)
		sprite.quads->color[i] = uint8_t(clamp(color[i] * 255.0f + 0.5f, 0.0f, 255.0f));

	sprite.quads->layer = position.z;
	sprite.quads->pos_off_x = int16_t(position.x);
	sprite.quads->pos_off_y = int16_t(position.y);
	sprite.quads->pos_scale_x = size.x;
	sprite.quads->pos_scale_y = size.y;
	sprite.quads->tex_off_x = tex_offset.x;
	sprite.quads->tex_off_y = tex_offset.y;
	sprite.quads->tex_scale_x = size.x;
	sprite.quads->tex_scale_y = size.y;

	sprite.render = RenderFunctions::sprite_render;

	Util::Hasher hasher;
	hasher.pointer(texture);
	hasher.u32(ecast(pipeline));
	sprite.instance_key = hasher.get();
	sprite.sorting_key = sprite.get_sprite_sort_key(queue_type, hasher.get(), position.z);
}
}