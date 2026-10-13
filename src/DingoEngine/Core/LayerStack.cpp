#include "depch.h"
#include "DingoEngine/Core/LayerStack.h"
#include "DingoEngine/Core/Layer.h"

namespace Dingo::Internal
{

	void LayerStack::Clear()
	{
		for (Layer* layer : m_Layers)
		{
			layer->OnDetach();
			delete layer;
		}
		m_Layers.clear();
		m_LayerInsertIndex = 0;
	}

	bool LayerStack::Empty() const
	{
		return m_Layers.empty();
	}

	void LayerStack::PushLayer(Layer* layer)
	{
		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
		m_LayerInsertIndex++;
	}

	void LayerStack::PushOverlay(Layer* overlay)
	{
		m_Layers.emplace_back(overlay);
	}

}
