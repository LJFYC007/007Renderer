#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "Environment.h"
#include "RenderPasses/RenderGraph.h"

namespace
{
class TestRenderPass : public RenderPass
{
public:
    TestRenderPass(
        ref<Device> pDevice,
        std::string name,
        std::vector<RenderPassInput> inputs,
        std::vector<RenderPassOutput> outputs,
        std::shared_ptr<std::vector<std::string>> executionLog
    )
        : RenderPass(pDevice)
        , mName(std::move(name))
        , mInputs(std::move(inputs))
        , mOutputs(std::move(outputs))
        , mpExecutionLog(std::move(executionLog))
    {}

    RenderData execute(const RenderData& input) override
    {
        if (mpExecutionLog)
            mpExecutionLog->push_back(mName);

        RenderData result;
        for (const auto& output : mOutputs)
            result.setResource(output.name, nullptr);

        mLastInputs = input;
        return result;
    }

    void renderUI() override {}

    std::vector<RenderPassInput> getInputs() const override { return mInputs; }
    std::vector<RenderPassOutput> getOutputs() const override { return mOutputs; }
    std::string getName() const override { return mName; }

    const RenderData& getLastInputs() const { return mLastInputs; }

private:
    std::string mName;
    std::vector<RenderPassInput> mInputs;
    std::vector<RenderPassOutput> mOutputs;
    std::shared_ptr<std::vector<std::string>> mpExecutionLog;
    RenderData mLastInputs;
};
} // namespace

class RenderGraphTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mpDevice = BasicTestEnvironment::getDevice();
        ASSERT_NE(mpDevice, nullptr);
        ASSERT_TRUE(mpDevice->isValid());
    }

    ref<Device> mpDevice;
};

TEST_F(RenderGraphTest, BuildsLinearGraph)
{
    auto executionLog = std::make_shared<std::vector<std::string>>();

    auto passA = make_ref<TestRenderPass>(
        mpDevice, "Source", std::vector<RenderPassInput>{}, std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}}, executionLog
    );
    auto passB = make_ref<TestRenderPass>(
        mpDevice,
        "Intermediate",
        std::vector<RenderPassInput>{{"input", RenderDataType::Texture2D}},
        std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}},
        executionLog
    );
    auto passC = make_ref<TestRenderPass>(
        mpDevice,
        "Sink",
        std::vector<RenderPassInput>{{"input", RenderDataType::Texture2D}},
        std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}},
        executionLog
    );

    std::vector<RenderGraphNode> nodes{
        {"A", passA},
        {"B", passB},
        {"C", passC},
    };

    std::vector<RenderGraphConnection> connections{
        {"A", "color", "B", "input"},
        {"B", "color", "C", "input"},
    };

    auto graph = RenderGraph::create(mpDevice, nodes, connections);
    ASSERT_NE(graph, nullptr);
    ASSERT_EQ(graph->getExecutionOrder().size(), 3);

    graph->execute();
    ASSERT_EQ(executionLog->size(), 3);
    EXPECT_EQ((*executionLog)[0], "Source");
    EXPECT_EQ((*executionLog)[1], "Intermediate");
    EXPECT_EQ((*executionLog)[2], "Sink");
}

TEST_F(RenderGraphTest, RejectsDuplicateNodeNames)
{
    auto executionLog = std::make_shared<std::vector<std::string>>();

    auto passA = make_ref<TestRenderPass>(
        mpDevice, "PassA", std::vector<RenderPassInput>{}, std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}}, executionLog
    );
    auto passB = make_ref<TestRenderPass>(
        mpDevice, "PassB", std::vector<RenderPassInput>{}, std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}}, executionLog
    );

    std::vector<RenderGraphNode> nodes{
        {"Duplicate", passA},
        {"Duplicate", passB},
    };

    std::vector<RenderGraphConnection> connections;

    auto graph = RenderGraph::create(mpDevice, nodes, connections);
    EXPECT_EQ(graph, nullptr);
}

TEST_F(RenderGraphTest, RejectsMissingRequiredInput)
{
    auto executionLog = std::make_shared<std::vector<std::string>>();

    auto passA = make_ref<TestRenderPass>(
        mpDevice, "Source", std::vector<RenderPassInput>{}, std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}}, executionLog
    );
    auto passB = make_ref<TestRenderPass>(
        mpDevice,
        "Sink",
        std::vector<RenderPassInput>{{"input", RenderDataType::Texture2D}},
        std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}},
        executionLog
    );

    std::vector<RenderGraphNode> nodes{
        {"Source", passA},
        {"Sink", passB},
    };

    std::vector<RenderGraphConnection> connections;

    auto graph = RenderGraph::create(mpDevice, nodes, connections);
    EXPECT_EQ(graph, nullptr);
}

TEST_F(RenderGraphTest, AllowsOptionalInputsToRemainUnconnected)
{
    auto executionLog = std::make_shared<std::vector<std::string>>();

    auto passA = make_ref<TestRenderPass>(
        mpDevice, "Source", std::vector<RenderPassInput>{}, std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}}, executionLog
    );
    auto passB = make_ref<TestRenderPass>(
        mpDevice,
        "Sink",
        std::vector<RenderPassInput>{{"input", RenderDataType::Texture2D, true}},
        std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}},
        executionLog
    );

    std::vector<RenderGraphNode> nodes{
        {"Source", passA},
        {"Sink", passB},
    };

    std::vector<RenderGraphConnection> connections;

    auto graph = RenderGraph::create(mpDevice, nodes, connections);
    ASSERT_NE(graph, nullptr);
    graph->execute();
}

TEST_F(RenderGraphTest, RejectsCycles)
{
    auto executionLog = std::make_shared<std::vector<std::string>>();

    auto passA = make_ref<TestRenderPass>(
        mpDevice,
        "A",
        std::vector<RenderPassInput>{{"input", RenderDataType::Texture2D}},
        std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}},
        executionLog
    );
    auto passB = make_ref<TestRenderPass>(
        mpDevice,
        "B",
        std::vector<RenderPassInput>{{"input", RenderDataType::Texture2D}},
        std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}},
        executionLog
    );

    std::vector<RenderGraphNode> nodes{
        {"A", passA},
        {"B", passB},
    };

    std::vector<RenderGraphConnection> connections{
        {"A", "color", "B", "input"},
        {"B", "color", "A", "input"},
    };

    auto graph = RenderGraph::create(mpDevice, nodes, connections);
    EXPECT_EQ(graph, nullptr);
}

TEST_F(RenderGraphTest, RejectsConnectionsToUnknownSlots)
{
    auto executionLog = std::make_shared<std::vector<std::string>>();

    auto passA = make_ref<TestRenderPass>(
        mpDevice, "Producer", std::vector<RenderPassInput>{}, std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}}, executionLog
    );
    auto passB = make_ref<TestRenderPass>(
        mpDevice,
        "Consumer",
        std::vector<RenderPassInput>{{"expected", RenderDataType::Texture2D}},
        std::vector<RenderPassOutput>{{"color", RenderDataType::Texture2D}},
        executionLog
    );

    std::vector<RenderGraphNode> nodes{
        {"Producer", passA},
        {"Consumer", passB},
    };

    std::vector<RenderGraphConnection> connections{
        {"Producer", "nonexistent", "Consumer", "expected"},
    };

    auto graph = RenderGraph::create(mpDevice, nodes, connections);
    EXPECT_EQ(graph, nullptr);
}
