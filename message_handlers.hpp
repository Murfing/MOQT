#pragma once

#include <moqt.hpp>
#include <serialization.hpp>

namespace rvn
{

/**
 * @brief MessageHandler class handles different types of MOQT (Media Over QUIC Transport) protocol messages
 * @tparam MOQTObject The type of MOQT object this handler will work with
 *
 * This class implements the message handling logic for the MOQT protocol, managing
 * the communication between publishers and subscribers in a QUIC-based media transport system.
 */
template <typename MOQTObject> class MessageHandler
{
    MOQTObject& moqt;                  // Reference to the main MOQT object
    ConnectionState& connectionState;   // Reference to the current connection state

    /**
     * @brief Handles the initial setup message from a client
     * @param connectionState Current connection state
     * @param clientSetupMessage The setup message received from the client
     * @return QUIC_STATUS indicating success or failure
     *
     * Process:
     * 1. Verifies version compatibility between client and server
     * 2. Extracts connection parameters (path, peer role)
     * 3. Responds with a SERVER_SETUP message
     */
    QUIC_STATUS
    handle_message(ConnectionState&, protobuf_messages::ClientSetupMessage&& clientSetupMessage)
    {
        // Check if the client supports our version
        auto& supportedversions = clientSetupMessage.supportedversions();
        auto matchingVersionIter =
            std::find(supportedversions.begin(), supportedversions.end(), moqt.version);

        // If no matching version is found, reject the connection
        if (matchingVersionIter == supportedversions.end())
        {
            // TODO: Implement connection destruction
            return QUIC_STATUS_INVALID_PARAMETER;
        }

        // Get the index of the matching version to find corresponding parameters
        std::size_t iterIdx = std::distance(supportedversions.begin(), matchingVersionIter);
        auto& params = clientSetupMessage.parameters()[iterIdx];
        
        // Store connection parameters
        connectionState.path = std::move(params.path().path());
        connectionState.peerRole = params.role().role();

        utils::LOG_EVENT(std::cout, "Client Setup Message received: \n",
                        clientSetupMessage.DebugString());

        // Prepare and send SERVER_SETUP response
        protobuf_messages::MessageHeader serverSetupHeader;
        serverSetupHeader.set_messagetype(protobuf_messages::MoQtMessageType::SERVER_SETUP);

        protobuf_messages::ServerSetupMessage serverSetupMessage;
        serverSetupMessage.add_parameters()->mutable_role()->set_role(
            protobuf_messages::Role::Publisher);

        // Serialize and queue the response
        QUIC_BUFFER* quicBuffer =
            serialization::serialize(serverSetupHeader, serverSetupMessage);
        connectionState.expectControlStreamShutdown = false;

        connectionState.enqueue_control_buffer(quicBuffer);

        return QUIC_STATUS_SUCCESS;
    }

    /**
     * @brief Handles the setup message from a server
     * @param connectionState Current connection state
     * @param serverSetupMessage The setup message received from the server
     * @return QUIC_STATUS indicating success or failure
     *
     * Validates and processes the server's setup parameters, ensuring they meet protocol requirements
     */
    QUIC_STATUS
    handle_message(ConnectionState&, protobuf_messages::ServerSetupMessage&& serverSetupMessage)
    {
        // Verify server doesn't use path parameter (protocol requirement)
        utils::ASSERT_LOG_THROW(connectionState.path.size() == 0,
                               "Server must not use the path parameter");
        
        // Ensure server provided at least role parameter
        utils::ASSERT_LOG_THROW(serverSetupMessage.parameters().size() > 0,
                               "SERVER_SETUP sent no parameters, requires at least role parameter");

        utils::LOG_EVENT(std::cout, "Server Setup Message received: ",
                        serverSetupMessage.DebugString());

        // Store server's role and mark control stream for shutdown
        connectionState.peerRole = serverSetupMessage.parameters()[0].role().role();
        connectionState.expectControlStreamShutdown = true;

        return QUIC_STATUS_SUCCESS;
    }

    /**
     * @brief Handles subscription requests from clients
     * @param connectionState Current connection state
     * @param subscribeMessage The subscription request message
     * @return QUIC_STATUS indicating success or failure
     *
     * Processes subscription requests for media content. Currently contains commented-out
     * validation logic that would verify proper message format and parameters.
     */
    QUIC_STATUS handle_message(ConnectionState&, protobuf_messages::SubscribeMessage&& subscribeMessage)
    {
        // NOTE: Commented out validation checks would verify:
        // - Client is subscribing to a Publisher
        // - Proper inclusion of start/end group/object IDs based on filter type
        // - Correct parameter combinations for different filter types

        utils::LOG_EVENT(std::cout, "Subscribe Message received: \n",
                        subscribeMessage.DebugString());

        // Register the subscription with the MOQT object
        auto err =
            moqt.try_register_subscription(connectionState, std::move(subscribeMessage));

        return QUIC_STATUS_SUCCESS;
    }

    /**
     * @brief Handles incoming media object stream messages
     * @param connectionState Current connection state
     * @param objectStreamMessage The message containing media object data
     * @return QUIC_STATUS indicating success or failure
     *
     * Processes incoming media object data and adds it to the appropriate queue
     */
    QUIC_STATUS
    handle_message(ConnectionState& connectionState,
                  protobuf_messages::ObjectStreamMessage&& objectStreamMessage)
    {
        std::uint64_t subscribeId = objectStreamMessage.subscribeid();
        connectionState.add_to_queue(objectStreamMessage.objectpayload());

        return QUIC_STATUS_SUCCESS;
    }

public:
    /**
     * @brief Constructor for MessageHandler
     * @param moqt Reference to the MOQT object
     * @param connectionState Reference to the connection state
     */
    MessageHandler(MOQTObject& moqt, ConnectionState& connectionState)
        : moqt(moqt), connectionState(connectionState)
    {
    }

    /**
     * @brief Generic message handler that deserializes and processes incoming messages
     * @tparam MessageType The type of message to be handled
     * @param connectionState Current connection state
     * @param istream Input stream containing the serialized message
     * @return QUIC_STATUS indicating success or failure
     */
    template <typename MessageType>
    QUIC_STATUS handle_message(ConnectionState& connectionState,
                             google::protobuf::io::IstreamInputStream& istream)
    {
        // Deserialize the message and forward to appropriate handler
        MessageType message = serialization::deserialize<MessageType>(istream);
        return handle_message(connectionState, std::move(message));
    }
};

} // namespace rvn
