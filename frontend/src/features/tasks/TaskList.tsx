import { AlertCircle, Ban, CheckCircle2, Clock3, LoaderCircle, X } from 'lucide-react'
import { Badge } from '../../components/ui/badge'
import { Progress } from '../../components/ui/progress'
import type { TaskSnapshot } from '../../platform/BeamDropApi'

export function TaskList({ tasks, onCancel }: { tasks: readonly TaskSnapshot[]; onCancel: (taskId: string) => void }) {
  const ordered = [...tasks].sort((a, b) => b.createdAt.localeCompare(a.createdAt))
  return <section className="rounded-lg border border-border bg-card"><div className="flex items-center justify-between border-b border-border px-5 py-4"><div><h2 className="text-base font-semibold">本次任务</h2><p className="mt-1 text-sm text-muted-foreground">通过 REST 快照恢复，实时事件只负责增量更新。</p></div><Badge>{ordered.length}</Badge></div>{ordered.length === 0 ? <div className="px-5 py-12 text-center text-sm text-muted-foreground">当前运行没有传输任务。</div> : <ul className="divide-y divide-border">{ordered.map((task) => <TaskRow key={task.taskId} task={task} onCancel={onCancel} />)}</ul>}</section>
}

function TaskRow({ task, onCancel }: { task: TaskSnapshot; onCancel: (taskId: string) => void }) {
  const progress = task.progress
  const percentage = progress?.totalBytes && progress.transferredBytes !== null ? progress.transferredBytes / progress.totalBytes * 100 : progress?.currentFileTotalBytes ? progress.currentFileBytes / progress.currentFileTotalBytes * 100 : 0
  const Icon = task.state === 'completed' ? CheckCircle2 : task.state === 'failed' ? AlertCircle : task.state === 'canceled' ? Ban : task.state === 'running' || task.state === 'canceling' ? LoaderCircle : Clock3
  const cancellable = task.state === 'pending' || task.state === 'running'
  return <li className="grid gap-3 px-5 py-4"><div className="flex items-start justify-between gap-3"><div className="min-w-0"><p className="truncate text-sm font-medium">{task.request.sourcePath}</p><p className="mt-1 text-xs text-muted-foreground">{task.request.targetHost}:{task.request.targetPort}{progress?.relativePath ? ` · ${progress.relativePath}` : ''}</p></div><div className="flex items-center gap-2">{cancellable && <button type="button" className="inline-flex h-7 w-7 items-center justify-center rounded border border-destructive/30 text-destructive" aria-label="取消任务" title="取消任务" onClick={() => onCancel(task.taskId)}><X size={15} /></button>}<Badge className={task.state === 'failed' ? 'border-destructive/30 bg-[#fef0ee] text-destructive' : task.state === 'completed' ? 'border-primary/30 bg-[#e3f3ec] text-primary' : ''}><Icon className={task.state === 'running' || task.state === 'canceling' ? 'animate-spin' : ''} size={13} />{task.state}</Badge></div></div>{progress && <div className="grid gap-1.5"><Progress value={percentage} /><p className="text-xs text-muted-foreground">{formatBytes(progress.transferredBytes ?? progress.currentFileBytes)} / {formatBytes(progress.totalBytes ?? progress.currentFileTotalBytes)}</p></div>}{task.error && <p className="text-sm text-destructive">{task.error.message}</p>}</li>
}

function formatBytes(value: number) { return value < 1024 ? `${value} B` : value < 1024 ** 2 ? `${(value / 1024).toFixed(1)} KB` : `${(value / 1024 ** 2).toFixed(1)} MB` }