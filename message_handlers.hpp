#pragma once

#include <moqt.hpp>
#include <serialization.hpp>

namespace rvn
{

/**
 * @brief Template class for handling various types of messages in a QUIC-based Media over QUIC Transport (MOQT) system.
 * 
 * This class serves as a central message processing hub that handles different types of protocol messages
 * such as client setup, server setup, subscriptions, and data streams. It uses template metaprogramming
 * to support flexibility in the underlying MOQT implementation.
 *
 * @tparam MOQTObject The core object type that implements the MOQT protocol operations
 */
template <typename MOQTObject> 
class MessageHandler
{
    MOQTObject& moqt;                // Reference to the core MOQT object containing protocol implementation
    ConnectionState& connectionState; // Manages the current state of the QUIC connection

    /**
     * @brief Processes a ClientSetupMessage received from a connecting client
     * 
     * This handler performs version negotiation between client and server:
     * 1. Checks if the client supports the server's protocol version
     * 2. Extracts connection parameters like path and role
     * 3. Responds with a ServerSetupMessage containing server configuration
     * 
     * @param connectionState Current connection state object
     * @param clientSetupMessage The received setup message from client
     * @return QUIC_STATUS Success or failure status code
     */
    QUIC_STATUS handle_message(ConnectionState&, protobuf_messages::ClientSetupMessage&& clientSetupMessage)
    {
        // Get list of protocol versions the client supports
        auto& supportedversions = clientSetupMessage.supportedversions();

        // Check if client supports our protocol version
        auto matchingVersionIter =
            std::find(supportedversions.begin(), supportedversions.end(), moqt.version);

        // If no matching version found, we can't communicate with this client
        if (matchingVersionIter == supportedversions.end())
        {
            // Future: Implement proper connection termination
            // connectionState.destroy_connection();
            return QUIC_STATUS_INVALID_PARAMETER;
        }

        // Get index of matching version in client's supported versions list
        std::size_t iterIdx = std::distance(supportedversions.begin(), matchingVersionIter);

        // Extract connection parameters for the matching version
        auto& params = clientSetupMessage.parameters()[iterIdx];

        // Set up connection path based on client parameters
        connectionState.path = std::move(params.path().path());

        // Store the role (Publisher/Subscriber) that the client wants to assume
        connectionState.peerRole = params.role().role();

        // Log the setup message for debugging
        utils::LOG_EVENT(std::cout, "Client Setup Message received: \n",
                         clientSetupMessage.DebugString());

        // Prepare server's response message
        protobuf_messages::MessageHeader serverSetupHeader;
        serverSetupHeader.set_messagetype(protobuf_messages::MoQtMessageType::SERVER_SETUP);

        // Create and configure server setup message
        protobuf_messages::ServerSetupMessage serverSetupMessage;
        serverSetupMessage.add_parameters()->mutable_role()->set_role(
            protobuf_messages::Role::Publisher);

        // Serialize the response into a QUIC buffer
        QUIC_BUFFER* quicBuffer =
            serialization::serialize(serverSetupHeader, serverSetupMessage);

        // Reset control stream shutdown flag
        connectionState.expectControlStreamShutdown = false;

        // Queue the response to be sent
        connectionState.enqueue_control_buffer(quicBuffer);

        return QUIC_STATUS_SUCCESS;
    }

    /**
     * @brief Processes a ServerSetupMessage received from a server
     * 
     * Validates and processes the server's setup parameters:
     * 1. Ensures server isn't using path parameter (which is client-only)
     * 2. Verifies required parameters are present
     * 3. Sets up connection state based on server's role
     * 
     * @param connectionState Current connection state object
     * @param serverSetupMessage The received setup message from server
     * @return QUIC_STATUS Success or failure status code
     */
    QUIC_STATUS handle_message(ConnectionState&, protobuf_messages::ServerSetupMessage&& serverSetupMessage)
    {
        // Server must not specify a path (that's client-only)
        utils::ASSERT_LOG_THROW(connectionState.path.size() == 0,
                                "Server must not use the path parameter");

        // Server must provide at least role parameter
        utils::ASSERT_LOG_THROW(serverSetupMessage.parameters().size() > 0,
                                "SERVER_SETUP sent no parameters, requires at least role parameter");

        // Log received message
        utils::LOG_EVENT(std::cout, "Server Setup Message received: ",
                         serverSetupMessage.DebugString());

        // Store server's declared role
        connectionState.peerRole = serverSetupMessage.parameters()[0].role().role();

        // Mark that we expect the control stream to close
        connectionState.expectControlStreamShutdown = true;

        return QUIC_STATUS_SUCCESS;
    }

    /**
     * @brief Handles subscription requests from clients
     * 
     * Processes a client's request to subscribe to specific data streams:
     * 1. Logs the subscription request
     * 2. Attempts to register the subscription with the MOQT core
     * 
     * @param connectionState Current connection state object
     * @param subscribeMessage The subscription request message
     * @return QUIC_STATUS Success or failure status code
     */
    QUIC_STATUS handle_message(ConnectionState&, protobuf_messages::SubscribeMessage&& subscribeMessage)
    {
        // Log subscription request
        utils::LOG_EVENT(std::cout, "Subscribe Message received: \n",
                         subscribeMessage.DebugString());

        // Attempt to register subscription with MOQT core
        auto err =
            moqt.try_register_subscription(connectionState, std::move(subscribeMessage));

        return QUIC_STATUS_SUCCESS;
    }

    /**
     * @brief Processes incoming data stream messages
     * 
     * Handles messages containing actual data payloads:
     * 1. Extracts the subscription ID
     * 2. Queues the payload for processing
     * 
     * @param connectionState Current connection state object
     * @param objectStreamMessage Message containing the data payload
     * @return QUIC_STATUS Success or failure status code
     */
    QUIC_STATUS handle_message(ConnectionState& connectionState,
                               protobuf_messages::ObjectStreamMessage&& objectStreamMessage)
    {
        // Get subscription ID this data belongs to
        std::uint64_t subscribeId = objectStreamMessage.subscribeid();

        // Queue payload for processing
        connectionState.add_to_queue(objectStreamMessage.objectpayload());

        return QUIC_STATUS_SUCCESS;
    }

public:
    /**
     * @brief Constructs a new MessageHandler instance
     * 
     * @param moqt Reference to the MOQT core object
     * @param connectionState Reference to the connection state
     */
    MessageHandler(MOQTObject& moqt, ConnectionState& connectionState)
        : moqt(moqt), connectionState(connectionState)
    {
    }

    /**
     * @brief Generic entry point for processing any type of message
     * 
     * Template function that:
     * 1. Deserializes incoming message stream
     * 2. Routes to appropriate handler based on message type
     * 
     * @tparam MessageType The type of message to be processed
     * @param connectionState Current connection state
     * @param istream Input stream containing the serialized message
     * @return QUIC_STATUS Success or failure status code
     */
    template <typename MessageType>
    QUIC_STATUS handle_message(ConnectionState& connectionState,
                               google::protobuf::io::IstreamInputStream& istream)
    {
        // Deserialize the message from the input stream
        MessageType message = serialization::deserialize<MessageType>(istream);

        // Route to appropriate handler based on message type
        return handle_message(connectionState, std::move(message));
    }
};

} // namespace rvn
