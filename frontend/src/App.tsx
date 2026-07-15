import { Activity, CircleAlert, CircleCheck, RadioTower } from "lucide-react";
import { Badge } from "./components/ui/badge";
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
    <main className="min-h-screen">
      <header className="border-b border-border bg-card">
        <div className="mx-auto flex max-w-6xl items-center justify-between gap-4 px-4 py-4 sm:px-6">
          <div className="flex items-center gap-3">
            <div className="grid h-9 w-9 place-items-center rounded-md bg-primary text-primary-foreground">
              <RadioTower size={19} />
            </div>
            <div>
              <h1 className="text-base font-semibold">BeamDrop 控制台</h1>
              <p className="text-xs text-muted-foreground">本机传输服务</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <Badge
              className={
                online
                  ? "border-primary/30 bg-[#e3f3ec] text-primary"
                  : state.connection === "offline"
                    ? "border-destructive/30 bg-[#fef0ee] text-destructive"
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
                  ? "border-primary/30 bg-[#e3f3ec] text-primary"
                  : ""
              }
            >
              {receiver.state === "running" ? <Activity size={13} /> : null}接收{" "}
              {receiver.state}
            </Badge>
          </div>
        </div>
      </header>
      <div className="mx-auto grid max-w-6xl gap-5 px-4 py-6 sm:px-6 lg:grid-cols-2">
        <div className="grid content-start gap-5">
          <SendPanel onSend={send} busy={pendingAction === "send"} />
          <ReceiverPanel
            receiver={receiver}
            onStart={startReceiver}
            onStop={stopReceiver}
            busy={pendingAction === "receiver"}
          />
        </div>
        <div className="grid content-start gap-5">
          <TaskList tasks={state.snapshot?.tasks ?? []} onCancel={cancel} />
          {state.lastError && (
            <div
              role="alert"
              className="rounded-md border border-destructive/30 bg-[#fef0ee] p-4 text-sm text-destructive"
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
