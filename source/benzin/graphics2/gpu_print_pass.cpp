#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/gpu_print_pass.hpp>

#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

namespace benzin
{

    template <typename T, typename HintT = T>
    static void ParseGpuPrintTypedArg(BufferReader& recordReader, std::string_view typeName, std::string& printRecord)
    {
        if (!printRecord.empty())
        {
            printRecord += ", ";
        }

        const auto componentCount = recordReader.ReadRaw<uint8_t>();

        if (componentCount == 1)
        {
            const HintT value = recordReader.ReadRaw<T>();
            std::format_to(std::back_inserter(printRecord), "{}: {}", typeName, value);
        }
        else
        {
            HintT values[4] = {};
            for (uint8_t componentIndex = 0; componentIndex < componentCount; ++componentIndex)
            {
                values[componentIndex] = recordReader.ReadRaw<T>();
            }

            std::format_to(std::back_inserter(printRecord), "{}{}: {}", typeName, componentCount, ToSpan(values, componentCount));
        }
    }

    static std::string ParseGpuPrintRecord(BufferReader& recordReader, uint32_t argCount)
    {
        std::string printRecord;
        printRecord.reserve(128);

        for (uint32_t argIndex = 0; argIndex < argCount; ++argIndex)
        {
            const auto argCode = (joint::GpuPrintArgCode)recordReader.ReadRaw<uint8_t>();

            switch (argCode)
            {
                case joint::GpuPrintArgCode::Uint:
                {
                    ParseGpuPrintTypedArg<uint32_t>(recordReader, "uint", printRecord);
                    break;
                }
                case joint::GpuPrintArgCode::Int:
                {
                    ParseGpuPrintTypedArg<int32_t>(recordReader, "int", printRecord);
                    break;
                }
                case joint::GpuPrintArgCode::Float:
                {
                    ParseGpuPrintTypedArg<float>(recordReader, "float", printRecord);
                    break;
                }
                case joint::GpuPrintArgCode::Bool:
                {
                    ParseGpuPrintTypedArg<uint32_t, bool>(recordReader, "bool", printRecord);
                    break;
                }
                case joint::GpuPrintArgCode::NewLine:
                {
                    printRecord.push_back('\n');
                    break;
                }
            }
        }

        return printRecord;
    }

    static std::vector<std::string> ParseGpuPrintRecords(BufferReader& reader)
    {
        std::vector<std::string> printRecords;

        const uint32_t writtenBufferSizeInBytes = reader.ReadRaw<uint32_t>();
        reader.SetBufferSizeInBytes(reader.GetBufferPositionInBytes() + writtenBufferSizeInBytes);

        while (reader.IsValidToRead<joint::GpuPrintRecordHeader>())
        {
            const auto header = reader.ReadRaw<joint::GpuPrintRecordHeader>();
            BenzinAssert(header.m_SizeInBytes != 0);
            BenzinAssert(reader.IsValidToRead(header.m_SizeInBytes));

            BufferReader recordReader = reader.MakeSubReader(header.m_SizeInBytes);
            std::string printRecord = ParseGpuPrintRecord(recordReader, header.m_ArgCount);
            
            printRecords.push_back(std::move(printRecord));
        }

        return printRecords;
    }

    // GpuPrintPass

    GpuPrintPass::GpuPrintPass(GpuPrintData& printData)
        : m_PrintData{ printData }
    {
        constexpr auto printBufferSizeInBytes = (uint32_t)256_kb;

        m_UavBuffer = ms_Device->GetPersistentDefaultAllocator().AllocateBuffer([this](BufferCreation& creation)
        {
            creation.m_DebugName = "GpuPrint::UavBuffer";
            creation.m_Type = BufferType::Byte;
            creation.m_ElementCount = printBufferSizeInBytes;
            creation.m_IsUnorderedAccessAllowed = true;
        });

        m_ReadbackBuffer = ms_Device->GetPersistentReadbackAllocator().AllocateBuffer([this](BufferCreation& creation)
        {
            creation.m_DebugName = "GpuPrint::ReadbackBuffer";
            creation.m_Type = BufferType::Byte;
            creation.m_ElementCount = printBufferSizeInBytes * BENZIN_READBACK_LATENCY;
        });

        m_Consts.m_PrintBufferHeapIndex = m_UavBuffer->GetUav().GetGpuHeapIndex();
        m_Consts.m_PrintBufferSizeInBytes = printBufferSizeInBytes;
    }

    void GpuPrintPass::OnZeroFrameInit()
    {
        GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        cmdList.AddResourceBarrier(TransitionBarrier{ *m_UavBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS });
        cmdList.AddResourceBarrier(TransitionBarrier{ *m_ReadbackBuffer, D3D12_RESOURCE_STATE_COMMON }, true);
        cmdList.ClearUnorderedAccess(*m_UavBuffer, m_UavBuffer->GetUav(), {});
    }

    void GpuPrintPass::OnUpdate()
    {
        m_Consts.m_CursorPosition = m_PrintData.m_CursorPosition;
    }

    void GpuPrintPass::OnRender() const
    {
        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        ReadbackFromGpu(cmdList);

        cmdList.ClearUnorderedAccess(*m_UavBuffer, m_UavBuffer->GetUav(), {});

        const uint64_t constBufferGpuAddress = ms_Device->GetConstBufferAllocator().Allocate(m_Consts);
        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::GpuPrintConsts, constBufferGpuAddress);
        cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::GpuPrintConsts, constBufferGpuAddress);
    }

    void GpuPrintPass::ReadbackFromGpu(benzin::CopyCmdList& cmdList) const
    {
        const uint32_t bufferSizeInBytes = m_Consts.m_PrintBufferSizeInBytes;
        const uint64_t destOffsetInBytes = (ms_Device->GetCpuFrameIndex() % BENZIN_READBACK_LATENCY) * bufferSizeInBytes;
        const uint64_t readbackOffsetInBytes = ((ms_Device->GetCpuFrameIndex() + 1) % BENZIN_READBACK_LATENCY) * bufferSizeInBytes;

        cmdList.CopyBufferRegion(*m_ReadbackBuffer, destOffsetInBytes, *m_UavBuffer, 0, bufferSizeInBytes);

        m_ReadbackBuffer->MapReadbackData(readbackOffsetInBytes, bufferSizeInBytes, [&](const std::byte* mappedData)
        {
            BufferReader reader{ mappedData, bufferSizeInBytes };
            m_PrintData.m_PrintRecords = ParseGpuPrintRecords(reader);
        });
    }

    // GpuPrintTool

    GpuPrintTool::GpuPrintTool(const GpuPrintData& printData)
        : ImGuiTool{ "Debug/GpuPrint", KeyCode::F4 }
        , m_PrintData{ printData }
    {}

    void GpuPrintTool::DrawWindowContent()
    {
        ImGui::FmtText("Cursor position: [{}, {}]", m_PrintData.m_CursorPosition.x, m_PrintData.m_CursorPosition.y);

        for (const std::string_view gpuPrintRecord : m_PrintData.m_PrintRecords)
        {
            ImGui::Text(gpuPrintRecord.data());
        }
    }

}
