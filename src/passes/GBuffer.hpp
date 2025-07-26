#include <d3d11.h>
#include <wrl.h>


using namespace Microsoft::WRL;


class GBuffer
{


public:
	GBuffer();
	~GBuffer();

	void draw();

private:
	ComPtr<ID3D11Texture2D> t_albedo;
	ComPtr<ID3D11Texture2D> t_metallicRoughness;
	ComPtr<ID3D11Texture2D> t_depth;
	ComPtr<ID3D11Texture2D> t_normal;
	ComPtr<ID3D11Texture2D> t_position;
	ComPtr<ID3D11Texture2D> t_position;

	ComPtr<ID3D11RenderTargetView> rtv_albedo;
	ComPtr<ID3D11RenderTargetView> rtv_metallicRoughness;
	ComPtr<ID3D11RenderTargetView> rtv_depth;
	ComPtr<ID3D11RenderTargetView> rtv_normal;
	ComPtr<ID3D11RenderTargetView> rtv_position;
	ComPtr<ID3D11RenderTargetView> rtv_position;



};
