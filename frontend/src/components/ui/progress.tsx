import { cn } from '../../lib/utils'

export function Progress({ value, className }: { value: number; className?: string }) {
  return <div className={cn('h-1.5 overflow-hidden rounded-full bg-slate-700', className)}><div className="h-full rounded-full bg-cyan-400 shadow-[0_0_10px_rgba(34,211,238,0.8)] transition-[width] duration-300 ease-out" style={{ width: `${Math.max(0, Math.min(100, value))}%` }} /></div>
}