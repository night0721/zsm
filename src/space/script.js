const MAX_NAME = 32;
const ZSM_STA_AUTHORISED = 20;

const keypair = { pk: "", sk: "" };
let serverAddress = "";
let selectedUser = null;

// Perform key exchange with recipient
function client_kx(from, recipient) {
	let from_pk = sodium.crypto_sign_ed25519_pk_to_curve25519(from.pk);
	let to_pk = sodium.crypto_sign_ed25519_pk_to_curve25519(recipient);
	let from_sk = sodium.crypto_sign_ed25519_sk_to_curve25519(from.sk);
	let out = sodium.crypto_kx_server_session_keys(from_pk, from_sk, to_pk);
	return out.sharedRx;
}

function create_signature(data, sk) {
	let hash = sodium.crypto_generichash(sodium.crypto_generichash_BYTES, data, null);
	let signature = sodium.crypto_sign_detached(hash, sk);
	return signature;
}

// Parse Uint8Array to a packet object
function parse_packet(bytes) {
	const type = bytes[0];
	const length = new DataView(bytes.buffer, 1, 4).getUint32(0, true);
	const data = bytes.slice(5, 5 + length);
	const signature = bytes.slice(5 + length, 5 + length + sodium.crypto_sign_BYTES);
	let cipher_len = length - sodium.crypto_box_NONCEBYTES - MAX_NAME * 2;

	// Deconstruct "data"
	const from = data.slice(0, MAX_NAME);
	const to = data.slice(MAX_NAME, MAX_NAME * 2); // Remove null padding
	const nonce = data.slice(MAX_NAME * 2, MAX_NAME * 2 + sodium.crypto_box_NONCEBYTES); // Extract nonce
	let encrypted = data.slice(MAX_NAME * 2 + sodium.crypto_box_NONCEBYTES, MAX_NAME * 2 + sodium.crypto_box_NONCEBYTES + cipher_len);

	let from_hex = sodium.to_hex(from);

	// Derive shared key
	let from_pk = sodium.crypto_sign_ed25519_pk_to_curve25519(from);
	let to_pk = sodium.crypto_sign_ed25519_pk_to_curve25519(keypair.pk);
	let to_sk = sodium.crypto_sign_ed25519_sk_to_curve25519(keypair.sk);
	let sharedKey = sodium.crypto_kx_client_session_keys(to_pk, to_sk, from_pk).sharedTx;

	// Remove last 8 bytes as it is timestamp
	let timestamp = encrypted.slice(encrypted.length - 8, encrypted.length);
	encrypted = encrypted.slice(0, encrypted.length - 8);

	let decrypted = new TextDecoder().decode(sodium.crypto_aead_xchacha20poly1305_ietf_decrypt(null, encrypted, null, nonce, sharedKey));
	return {
		type,
		length,
		from: from_hex,
		data: decrypted,
		time: bytesToTime(timestamp),
		signature
	};
}

function timeToBytes(timestamp) {
	// Always use 8 bytes
    let buffer = new ArrayBuffer(8);
    let view = new DataView(buffer);
    view.setBigInt64(0, BigInt(timestamp), true);
    return new Uint8Array(buffer);
}

function bytesToTime(bytes) {
	let view = new DataView(bytes.buffer).getBigInt64(0, true);
	return Number(view);
}

// from: Object consisting publicKey and privateKey
// recipient: Username(64 digits)
// message: String
function create_message_packet(from, recipient, message) {
	let recipient_pk = sodium.from_hex(recipient);
	let nonce = sodium.randombytes_buf(sodium.crypto_box_NONCEBYTES);
	let sharedKey = client_kx(from, sodium.from_hex(recipient));
	let encrypted = sodium.crypto_aead_xchacha20poly1305_ietf_encrypt(message, null, null, nonce, sharedKey);

	let data = new Uint8Array(MAX_NAME * 2 + nonce.length + encrypted.length + 8);
	data.set(from.pk, 0);
	data.set(recipient_pk, MAX_NAME);
	data.set(nonce, MAX_NAME * 2);
	data.set(encrypted, MAX_NAME * 2 + nonce.length);
	// add unix timestamp to the end of the message 8 bytes
	let timeBytes = timeToBytes(Math.floor(Date.now() / 1000));
	data.set(timeBytes, MAX_NAME * 2 + nonce.length + encrypted.length);

	let signature = create_signature(data, from.sk);

	let packet = new Uint8Array(1 + 4 + data.length + signature.length);
	// Type
	packet[0] = 2;
	// Write the `length` as a 4-byte unsigned integer
	const view = new DataView(packet.buffer);
	view.setUint32(1, data.length, true);

	packet.set(data, 5);
	packet.set(signature, 5 + data.length);
	return packet;
}

function create_message_div(outgoing, author, content, time) {
	const messageDiv = document.createElement("div");
	messageDiv.classList.add("message");
	if (outgoing) {
		messageDiv.classList.add("outgoing");
	} else {
		messageDiv.classList.add("incoming");
	}

	// Add the author
	const authorElement = document.createElement("a");
	authorElement.classList.add("message-author");
	authorElement.textContent = author;

	// Add the message content
	const messageContent = document.createElement("a");
	messageContent.classList.add("message-content");
	messageContent.textContent = content;

	// Add the message time
	time = new Date(time * 1000);
	const hours = time.getHours().toString().padStart(2, "0");
	const minutes = time.getMinutes().toString().padStart(2, "0");
	const messageTime = document.createElement("a");
	messageTime.classList.add("message-time");
	messageTime.textContent = hours + ":" + minutes;

	// Append author, content, and time to the message container
	messageDiv.appendChild(authorElement);
	messageDiv.appendChild(messageContent);
	messageDiv.appendChild(messageTime);


	// Append the message to the messages container
	const messagesDiv = document.querySelector(".messages");
	messagesDiv.appendChild(messageDiv);

	// Scroll to the bottom
	messagesDiv.scrollTop = messagesDiv.scrollHeight;
}

function autoscale_height(element) {
	element.style.height = "1px";
	element.style.height = `${element.scrollHeight}px`;
}

function createUserDiv(user) {
	const userList = document.getElementById("users");
	const button = document.createElement("div");
	const container = document.createElement("div");
	const name = document.createElement("a");
	const lastMessage = document.createElement("a");
	const deleteChatButton = document.createElement("img");
	const changeNicknameButton = document.createElement("img");
	button.classList.add("user");
	container.classList.add("user-container");
	name.classList.add("user-name");
	name.dataset.name = user.username;
	lastMessage.classList.add("user-last-message");
	deleteChatButton.classList.add("user-button");
	deleteChatButton.classList.add("delete-chat");
	deleteChatButton.src = "./assets/minus.svg";
	deleteChatButton.alt = "Delete chat";
	deleteChatButton.title = "Delete chat";
	changeNicknameButton.classList.add("user-button");
	changeNicknameButton.classList.add("change-nickname");
	changeNicknameButton.src = "./assets/write.svg";
	changeNicknameButton.alt = "Change nickname";
	changeNicknameButton.title = "Change nickname";

	name.textContent = user.nickname;
	lastMessage.textContent = "";
	container.appendChild(name);
	container.appendChild(lastMessage);
	button.appendChild(container);
	button.appendChild(deleteChatButton);
	button.appendChild(changeNicknameButton);

	userList.appendChild(button);
	button.addEventListener("click", () => {
		for (let i = 0; i < userList.children.length; i++) {
			if (userList.children[i] != button) {
				userList.children[i].classList.remove("selected");
			}
		}
		button.classList.add("selected");

		if (button.classList.contains("unread")) {
			button.classList.remove("unread");
		}
		selectedUser = button.querySelector(".user-name").dataset.name;
		// Clear messages div
		const messagesDiv = document.querySelector(".messages");
		messagesDiv.innerHTML = "";
		showMessages(selectedUser);
	});

	deleteChatButton.addEventListener("click", () => {
		// Delete all messages with the selected user
		let messages = localStorage.getItem("messages");
		messages = JSON.parse(messages);
		// Filter out messages with the selected user
		messages = messages.filter((message) => message.from !== selectedUser && message.to !== selectedUser);
		localStorage.setItem("messages", JSON.stringify(messages));
		// Remove user from list
		let users = localStorage.getItem("users");
		users = JSON.parse(users);
		// Delete from dict
		delete users[user.username];
		localStorage.setItem("users", JSON.stringify(users));

		userList.removeChild(button);
	});
	
	changeNicknameButton.addEventListener("click", () => {
		let nickname = prompt("Enter new nickname:", name.textContent);
		if (!nickname) return;
		if (nickname.length > MAX_NAME * 2) {
			alert("Nickname is too long. Maximum length is 64 characters.");
			return;
		}
		name.textContent = nickname;
		// Save to local storage
		let users = localStorage.getItem("users");
		users = JSON.parse(users);
		users[user.username].nickname = nickname;
		localStorage.setItem("users", JSON.stringify(users));
	});
	return button;
}

function showMessages(user) {
	let messages = localStorage.getItem("messages");
	if (!messages) return;
	messages = JSON.parse(messages);
	messages.filter((message) => (message.from === user && message.to === sodium.to_hex(keypair.pk)) ||
		(message.from === sodium.to_hex(keypair.pk) && message.to === user)).forEach((message) => {
			create_message_div(message.from === sodium.to_hex(keypair.pk), message.from, message.message, message.time);
		});
}

function saveMessage(from, to, message, time) {
	let messages = localStorage.getItem("messages");
	messages = JSON.parse(messages);
	messages.push({ from, to, message, time });
	localStorage.setItem("messages", JSON.stringify(messages));
}

async function authenticate() {
	try {
		const ws = new WebSocket("wss://" + serverAddress);
		// DOM elements
		const inputField = document.getElementById("message-input");
		const sendButton = document.getElementById("send-button");

		// Send message when button is clicked
		sendButton.addEventListener("click", () => {
			let message = inputField.value;
			if (message != "" && selectedUser != null) {
				let packet = create_message_packet(keypair, selectedUser, message);
				ws.send(packet);
				create_message_div(true, sodium.to_hex(keypair.pk), message, Math.floor(Date.now() / 1000));
				saveMessage(sodium.to_hex(keypair.pk), selectedUser, message, Math.floor(Date.now() / 1000));
				inputField.value = null; // Clear input field after sending
			}
		});

		// Send message when Enter key is pressed
		inputField.addEventListener("keydown", (event) => {
			if (event.key === "Enter" && !event.shiftKey) {
				event.preventDefault();
				sendButton.click();
			}
		});

		let state = "unauthenticated";
		ws.onopen = () => {
			console.log("[DEBUG] Connected to WebSocket server");
			ws.onmessage = async (event) => {
				if (state == "unauthenticated") {
					// Authenticate with server by signing a challenge
					const bytes = new Uint8Array(await event.data.arrayBuffer());
					const type = bytes[0];
					const length = new DataView(bytes.buffer, 1, 4).getUint32(0, true);
					const data = bytes.slice(5, 5 + length);

					// Sign challenge with private key
					const sig = sodium.crypto_sign_detached(data, keypair.sk);

					// Create Packet
					const msg = new Uint8Array(1 + 4 + keypair.pk.length + sig.length);
					// Type
					msg[0] = 1;

					const view = new DataView(msg.buffer);
					view.setUint32(1, keypair.pk.length, true);

					msg.set(keypair.pk, 5);
					msg.set(sig, 5 + keypair.pk.length);
					ws.send(msg);
					state = "authenticating";
					return;
				}
				if (state == "authenticating") {
					const bytes = new Uint8Array(await event.data.arrayBuffer());
					// Check if server replies authorised or unauthorised
					if (bytes[0] == ZSM_STA_AUTHORISED) {
						state = "authenticated";
						document.getElementById("left-panel").style.display = "flex";
						document.getElementById("right-panel").style.display = "flex";
						document.getElementById("login").style.display = "none";
						document.getElementById("signup").style.display = "none";

						document.querySelector("body").style.alignItems = "normal";
						document.getElementById("current-username").textContent += sodium.to_hex(keypair.pk).slice(0, 6) + "..";
						return;
					} else {
						state = "unauthenticated";
						ws.close();
						alert(`Authentication failed ${bytes[0]}`);
						return;
					}
				}
				if (state == "authenticated") {
					const bytes = new Uint8Array(await event.data.arrayBuffer());
					const packet = parse_packet(bytes);
					saveMessage(packet.from, sodium.to_hex(keypair.pk), packet.data, packet.time);
					// Notify user of incoming message
					// Get user list
					let users = localStorage.getItem("users");
					users = JSON.parse(users);
					if (!users[packet.from]) {
						// Add user to list to notify user of incoming message from new user
						let button = createUserDiv({ username: packet.from, nickname: packet.from });
						button.classList.add("unread");
						users[packet.from] = packet.from;
						localStorage.setItem("users", JSON.stringify(users));
					}
					if (selectedUser == packet.from) {
						// If viewing message from the user, display the message
						create_message_div(false, packet.from, packet.data, packet.time);
					} else {
						// If not viewing message from the user, notify user of unread message
						let button = document.querySelector(`.user-name[data-name="${packet.from}"]`);
						button.parentElement.parentElement.classList.add("unread");
					}
				}
			};
		};

		ws.onerror = (err) => {
			console.error("WebSocket error:", err);
		};
	} catch (error) {
		console.error("Error during authentication:", error);
	}
}

// Login and signup form events
const loginForm = document.getElementById("login-form");
const signupForm = document.getElementById("signup-form");
const signupButton = document.getElementById("signup-button");
const loginLinkButton = document.getElementById("login-link");
const genereateKeysButton = document.getElementById("generate-keys");

const signupPublicKey = document.getElementById("signup-publickey");
const signupPrivateKey = document.getElementById("signup-privatekey");
const loginPublicKey = document.getElementById("login-publickey");
const loginPrivateKey = document.getElementById("login-privatekey");

const currentUsernameButton = document.getElementById("current-username");

const signoutButton = document.getElementById("signout");
const addUserButton = document.getElementById("add-chat");

signupPublicKey.value = "";
signupPrivateKey.value = "";
loginPublicKey.value = "";
loginPrivateKey.value = "";

signupButton.addEventListener("click", () => {
	document.getElementById("login").style.display = "none";
	document.getElementById("signup").style.display = "block";
});

genereateKeysButton.addEventListener("click", async (event) => {
	event.preventDefault();

	// Generate key pair
	const keypair = sodium.crypto_sign_keypair();

	const pk = sodium.to_hex(keypair.publicKey);
	const sk = sodium.to_hex(keypair.privateKey);
	// Set the public and private keys in the signup and login forms
	signupPublicKey.value = pk;
	signupPrivateKey.value = sk;
	loginPublicKey.value = pk;
	loginPrivateKey.value = sk;

	// Add a notice to save the keys
	const footer = document.getElementById("signup-footer");

	let notice = document.getElementById("save-keys-notice");
	let notice2 = document.getElementById("login-instruction-notice");

	if (!notice) {
		notice = document.createElement("p");
		notice.id = "save-keys-notice";
		notice.innerHTML = "Please save your keys in a safe place. You will <b style=\"color: white;\">not</b> be able to recover them if lost.";
		footer.appendChild(notice);
	}

	if (!notice2) {
		notice2 = document.createElement("p");
		notice2.id = "login-instruction-notice";
		notice2.innerHTML = "Press login to use the public key generated to login.";
		footer.appendChild(notice2);
	}
});

loginLinkButton.addEventListener("click", () => {
	document.getElementById("login").style.display = "block";
	document.getElementById("signup").style.display = "none";
	signupPublicKey.value = "";
	signupPrivateKey.value = "";
});

loginForm.addEventListener("submit", async (event) => {
	event.preventDefault();

	// Get the input values
	const publicKey = document.getElementById("login-publickey").value.trim();
	const privateKey = document.getElementById("login-privatekey").value.trim();
	const address = document.getElementById("login-server").value.trim();

	if (!publicKey || !privateKey) {
		alert("Please enter both public and private keys to login.");
		return;
	}
	if (publicKey.length != 64 || privateKey.length != 128) {
		alert("Invalid key length. Public key must have 64 characters and private key must have 128 characters.");
		return;
	}

	const rememberLogin = document.getElementById("remember-me").checked;
	if (rememberLogin) {
		localStorage.setItem("login", JSON.stringify({ pk: publicKey, sk: privateKey, address }));
	}
	keypair.pk = sodium.from_hex(publicKey);
	keypair.sk = sodium.from_hex(privateKey);
	serverAddress = address;
	authenticate();
});

// Left panel events
currentUsernameButton.addEventListener("click", async () => {
	await navigator.clipboard.writeText(sodium.to_hex(keypair.pk));
});

addUserButton.addEventListener("click", () => {
	// Prompt user for username
	const username = prompt("Enter the username to add:", "");
	if (username === null) return;
	if (username.length != 64) {
		alert("Invalid username length. Username must have 64 characters.");
		return;
	}
	// Add username to the list
	createUserDiv({ username, nickname: username });
	let users = localStorage.getItem("users");
	users = JSON.parse(users);
	users[username] = {nickname: username, username: username};
	localStorage.setItem("users", JSON.stringify(users));
});

signoutButton.addEventListener("click", () => {
	localStorage.removeItem("login");
	location.reload();
});

// wait for the DOM to be loaded
window.sodium = {
	onload: async function () {
		// Remember login
		const credentials = localStorage.getItem("login");
		if (credentials) {
			const { pk, sk, address } = JSON.parse(credentials);
			keypair.pk = sodium.from_hex(pk);
			keypair.sk = sodium.from_hex(sk);
			serverAddress = address;
			await authenticate();
		} else {
			// Display login form
			document.getElementById("login").style.display = "block";
		}

		// Initialize users
		const usersArr = localStorage.getItem("users");
		if (!usersArr) {
			localStorage.setItem("users", JSON.stringify({}));
		} else {
			const users = JSON.parse(usersArr);
			let first = false;
			for (let user in users) {
				const div = createUserDiv(users[user]);
				// Automatically select the first user
				if (!first) {
					div.classList.add("selected");
					selectedUser = users[user].username;
					first = true;
				}
			}
			showMessages(selectedUser);
		}

		// Initialize messages
		const messages = localStorage.getItem("messages");
		if (!messages) {
			localStorage.setItem("messages", JSON.stringify([]));
		}
	}
};

