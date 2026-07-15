import { Activity, CircleAlert, CircleCheck, RadioTower } from "lucide-react";
import { Badge } from "./components/ui/badge";
import { BackgroundPaths } from "./components/ui/background-paths";
import { useBeamDropDashboard } from "./features/dashboard/useBeamDropDashboard";
import { ReceiverPanel } from "./features/receiver/ReceiverPanel";
import { SendPanel } from "./features/send/SendPanel";
import { TaskList } from "./features/tasks/TaskList";
import { createBeamDropApi } from "./platform/createBeamDropApi";

const api = createBeamDropApi(
  import.meta.env.VITE_BEAMDROP_API_URL ?? "http://127.0.0.1:8000",
);
const emptyReceiver = {
  state: "stopped" as const,
  host: null,
  port: null,
  saveDir: null,
  lastError: null,
};
function App() {
  const { state, pendingAction, send, cancel, startReceiver, stopReceiver } =
    useBeamDropDashboard(api);
  const receiver = state.snapshot?.receiver ?? emptyReceiver;
  const online = state.connection === "online";
  return (
    <main className="monument-app relative min-h-screen overflow-hidden">
      <BackgroundPaths />
      <header className="monument-header relative z-10 border-b border-border">
        <div className="mx-auto flex max-w-6xl flex-wrap items-center justify-between gap-4 px-4 py-4 sm:px-6">
          <div className="flex min-w-0 items-center gap-3">
            <div className="monument-mark grid h-9 w-9 place-items-center bg-primary text-primary-foreground">
              <RadioTower size={19} />
            </div>
            <div>
              <p className="monument-kicker">LOCAL TRANSFER SYSTEM</p>
              <h1 className="monument-title">BeamDrop 控制台</h1>
            </div>
          </div>
          <div className="flex flex-wrap items-center gap-2">
            <Badge
              className={
                online
                  ? "border-cyan-400/30 bg-cyan-500/10 text-cyan-300"
                  : state.connection === "offline"
                    ? "border-rose-400/30 bg-rose-500/10 text-rose-300"
                    : ""
              }
            >
              {online ? <CircleCheck size={13} /> : <CircleAlert size={13} />}
              {online
                ? "后端已连接"
                : state.connection === "offline"
                  ? "后端离线"
                  : "正在连接"}
            </Badge>
            <Badge
              className={
                receiver.state === "running"
                  ? "border-emerald-400/30 bg-emerald-500/10 text-emerald-300"
                  : ""
              }
            >
              {receiver.state === "running" ? <Activity size={13} /> : null}接收{" "}
              {receiver.state}
            </Badge>
          </div>
        </div>
      </header>
      <div className="monument-content relative z-10 mx-auto grid min-w-0 max-w-6xl gap-0 px-4 py-6 sm:px-6 lg:grid-cols-2">
        <div className="grid min-w-0 content-start gap-5">
          <SendPanel onSend={send} busy={pendingAction === "send"} />
          <ReceiverPanel
            receiver={receiver}
            onStart={startReceiver}
            onStop={stopReceiver}
            busy={pendingAction === "receiver"}
          />
        </div>
        <div className="grid min-w-0 content-start gap-5">
          <TaskList tasks={state.snapshot?.tasks ?? []} onCancel={cancel} />
          {state.lastError && (
            <div
              role="alert"
              className="rounded-xl border border-rose-400/30 bg-rose-500/10 p-4 text-sm text-rose-300 shadow-lg backdrop-blur"
            >
              <p className="font-medium">{state.lastError.code}</p>
              <p className="mt-1">{state.lastError.message}</p>
            </div>
          )}
        </div>
      </div>
    </main>
  );
}

export default App;
