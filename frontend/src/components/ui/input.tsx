import type { InputHTMLAttributes } from 'react'
import { cn } from '../../lib/utils'

export function Input({ className, ...props }: InputHTMLAttributes<HTMLInputElement>) {
  return <input className={cn('h-10 w-full rounded-xl border border-slate-700 bg-slate-900/75 px-3 text-sm text-slate-100 shadow-inner outline-none placeholder:text-slate-500 transition-colors focus:border-cyan-400 focus:ring-2 focus:ring-cyan-400/20 disabled:bg-slate-800', className)} {...props} />
}