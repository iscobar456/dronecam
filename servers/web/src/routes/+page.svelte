<script lang="ts">
	import type { MessageBody, Node } from '$lib/types';
	import { onMount } from 'svelte';
	import { PUBLIC_ENVIRONMENT } from '$env/static/public';
	import Portal from './Portal.svelte';
	import RestartSvg from '$lib/assets/restart.svg';
	import EnterSvg from '$lib/assets/enter.svg';

	const WEBSOCKET_URL =
		PUBLIC_ENVIRONMENT == 'dev' ? 'ws://localhost:8080' : 'wss://dcsignaling.isaacspencer.com/';

	onMount(() => {
		main();
	});

	const id = crypto.randomUUID();
	let ws: WebSocket;
	let pc: RTCPeerConnection;
	let isConnected: boolean = $state(false);
	let stream: MediaStream | null = $state(null);
	let peerId = $state('');
	let iceQueue: Array<RTCIceCandidate> = $state([]);
	let remoteIceQueue: Array<RTCIceCandidate> = $state([]);
	let drones: Array<Node> = $state([]);

	function main() {
		initializeRtcPc();
		ws = new WebSocket(`${WEBSOCKET_URL}?id=${id}&type=observer`);
		ws.addEventListener('open', () => {
			refreshList();
		});
		ws.addEventListener('close', () => {
			// console.log('connection closed');
		});
		ws.addEventListener('message', messageHandler);
		ws.addEventListener('error', () => {});
	}

	function initializeRtcPc() {
		const config = {
			iceServers: [
				{
					urls: 'stun:stun.actionvoip.com:3478'
				},
				{
					urls: 'turn:standard.relay.metered.ca:80',
					username: 'bfc22cc224cb894f60cff28a',
					credential: 'KkdyHKXNNh8ptCyj'
				}
			]
		};

		pc = new RTCPeerConnection(config);
		pc.onicecandidate = handleIceCandidate;
		pc.onconnectionstatechange = () => {
			console.log('Connection state: ' + pc.connectionState);
			switch (pc.connectionState) {
				case 'connecting':
				case 'connected':
					isConnected = true;
					break;
				default:
					isConnected = false;
					stream = null;
					break;
			}
		};
		pc.ontrack = (ev: RTCTrackEvent) => {
			console.log('got track');
			stream = ev.streams[0] || null;
		};
		pc.addTransceiver('video', { direction: 'recvonly' });
	}

	const handleIceCandidate = (event: RTCPeerConnectionIceEvent) => {
		console.log('got an ice candidate');
		if (!event.candidate) {
			return;
		}
		if (event.candidate.candidate.includes(' tcp ')) return;
		if (event.candidate.candidate.includes('.local')) return;
		if (peerId) {
			sendMessage(ws, createIceMessage(event.candidate));
		} else {
			iceQueue.push(event.candidate);
		}
	};

	const createIceMessage = (candidate: RTCIceCandidate) => {
		const message = JSON.stringify({
			type: 'ice',
			body: {
				from: id,
				to: peerId,
				data: JSON.stringify(candidate)
			}
		});
		return message;
	};

	const refreshList = () => {
		console.log('refreshing list');
		const message = {
			type: 'list',
			body: {
				to: '',
				from: id,
				data: ''
			}
		};
		sendMessage(ws, JSON.stringify(message));
	};

	const messageHandler = async (event: MessageEvent) => {
		const message = JSON.parse(event.data);

		if (message.type === 'sd') {
			await handleOffer(message.body);
		} else if (message.type === 'ice') {
			handleIceMessage(message.body.data);
		} else if (message.type === 'list') {
			const res = JSON.parse(message.body.data);
			drones = res;
		} else if (message.type === 'disconnect') {
			disconnect(false);
		}
	};

	const disconnect = (sendNotif: boolean) => {
		if (sendNotif) {
			const message = {
				type: 'disconnect',
				body: {
					to: '',
					from: '',
					data: ''
				}
			};
			sendMessage(ws, JSON.stringify(message));
		}
		pc.close();
		initializeRtcPc();
		isConnected = false;
		stream = null;
	};

	const handleIceMessage = (cand: string) => {
		console.log('received an ice candidate: ' + cand);
		const candidate = new RTCIceCandidate({ candidate: cand, sdpMLineIndex: 0 });
		if (pc.remoteDescription != null) {
			pc.addIceCandidate(candidate);
		} else {
			remoteIceQueue.push(candidate);
		}
	};

	const handleOffer = async (offerMessage: MessageBody) => {
		console.log('Handling remote SDP');
		console.log(offerMessage.data);
		const sd = offerMessage.data;
		await pc.setRemoteDescription({ sdp: sd, type: 'offer' });
		const answer = await pc.createAnswer();
		await pc.setLocalDescription(answer);
		const message = {
			type: 'sd',
			body: {
				from: id,
				to: peerId,
				data: JSON.stringify(answer)
			}
		};
		sendMessage(ws, JSON.stringify(message));

		remoteIceQueue.forEach((candidate) => pc.addIceCandidate(candidate));
	};

	const connectToDrone = async (droneId: string) => {
		peerId = droneId;
		const message = {
			type: 'select',
			body: {
				from: id,
				to: peerId,
				data: ''
			}
		};
		sendMessage(ws, JSON.stringify(message));

		iceQueue.forEach((candidate) => {
			sendMessage(ws, createIceMessage(candidate));
		});
		iceQueue.length = 0;
	};

	const sendMessage = (ws: WebSocket, message: string) => {
		ws.send(message);
	};
</script>

<div class="h-screen w-full flex-col bg-mist-200 px-15 dark:bg-neutral-700">
	<div class="mx-auto max-w-lg">
		<div class="flex border-b pb-3">
			<h1 class="font-bolder mt-15 text-3xl">Drone List</h1>
			<button
				class="ml-auto box-content h-7 w-7 cursor-pointer self-end rounded-sm border px-2"
				onclick={refreshList}
				aria-label="Refresh drone list"
			>
				<img src={RestartSvg} alt="Refresh drone list icon" class="dark:invert" />
			</button>
		</div>
		<ol class="flex w-full flex-col gap-3 py-3">
			{#each drones as drone (drone.id)}
				<li class="w-full">
					<button
						onclick={() => {
							connectToDrone(drone.id);
						}}
						class="flex w-full cursor-pointer items-center p-3 hover:bg-black/5 dark:hover:bg-white/5"
					>
						<p>{drone.name}</p>
						<img class="ml-auto h-6 w-6 dark:invert" src={EnterSvg} alt="Connect to drone icon" />
					</button>
				</li>
			{/each}
		</ol>
	</div>
</div>

<Portal {stream} {disconnect} />
