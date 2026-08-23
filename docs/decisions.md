### Why and What am I trying to make ?
- I want to make a local version of wiaper_flow all by myself because I think that they don't desever shit.
- The project is broken into 4 parts :
    - part 1 : loacl wisperfolw for laptops and computers (lacal LLM, SST, ASR; option for connecting to your LLM server, minimal app and/or WebUI; ability to paste the text)
    - part 2 : application for phone (local SST, ASR; option for getting infrence from your LLM server, minimal app UI)
    - part 3 : Intigrating remote SST and ASR in both computer and phone application. Also collecting user data to improve LLM, SST, ASR (with user consent ofc).
    - Part 4 : Addition of Insigts, Dictionary, Snippets and Transforms. To both 


### Features and Working framework of Wisper flow + extra:
- ASR (Automatic Speech Recognition): It uses a highly optimized version of OpenAI's Whisper model (or a similar Context-Conditioned ASR) to convert raw audio waves into rough text.
- LLM (Large Language Model): It pipes that rough text through an LLM (like Claude or GPT) to format it, remove filler words (um, ah), fix grammar, and apply specific formatting (like bullet points or code blocks) before pasting it.
- Upstream (Audio to Cloud): It streams binary audio to their servers. This is where Proto / gRPC or WebSockets would be used to keep the connection open and the payload small.
- Downstream (Text to Device): The server sends back the processed text. This response is likely JSON, as it contains the final string, formatting metadata, and potential commands (e.g., "delete previous word").
- Desktop (Mac/Windows): It hooks into the operating system's Accessibility API to "paste" the text into whichever application is active (VS Code, Slack, Notion).
- Mobile (iOS/Android): It often runs as a custom software keyboard (iOS) or an overlay service (Android) to inject text directly into input fields.
- Insigts, Dictionary, Snippets and Transforms --> just prompting and bs features
- Translation

### Tech stack :
